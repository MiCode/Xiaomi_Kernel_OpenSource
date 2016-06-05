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

/**===========================================================================
  
  \file  wlan_hdd_tx_rx.c
  
  \brief Linux HDD Tx/RX APIs
  
  ==========================================================================*/
  
/*--------------------------------------------------------------------------- 
  Include files
  -------------------------------------------------------------------------*/ 
#include <wlan_hdd_tx_rx.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_dp_utils.h>
#include <wlan_qct_tl.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ratelimit.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
#include <soc/qcom/subsystem_restart.h>
#else
#include <mach/subsystem_restart.h>
#endif

#include <wlan_hdd_p2p.h>
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include <net/ieee80211_radiotap.h>
#include "sapApi.h"
#include <vos_sched.h>
#ifdef FEATURE_WLAN_TDLS
#include "wlan_hdd_tdls.h"
#endif

#include "vos_diag_core_event.h"
#include "vos_utils.h"
#include  "sapInternal.h"
#include  "wlan_hdd_trace.h"
#include  "wlan_qct_wda.h"
/*--------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/ 

const v_U8_t hddWmmAcToHighestUp[] = {
   SME_QOS_WMM_UP_RESV,
   SME_QOS_WMM_UP_EE,
   SME_QOS_WMM_UP_VI,
   SME_QOS_WMM_UP_NC
};

//Mapping Linux AC interpretation to TL AC.
const v_U8_t hdd_QdiscAcToTlAC[] = {
   WLANTL_AC_VO,
   WLANTL_AC_VI,
   WLANTL_AC_BE,
   WLANTL_AC_BK,
   WLANTL_AC_HIGH_PRIO,
};

#define HDD_TX_TIMEOUT_RATELIMIT_INTERVAL 20*HZ
#define HDD_TX_TIMEOUT_RATELIMIT_BURST    1
#define HDD_TX_STALL_SSR_THRESHOLD        5
#define HDD_TX_STALL_SSR_THRESHOLD_HIGH   13
#define HDD_TX_STALL_RECOVERY_THRESHOLD HDD_TX_STALL_SSR_THRESHOLD - 2
#define HDD_TX_STALL_KICKDXE_THRESHOLD  HDD_TX_STALL_SSR_THRESHOLD - 4
#define HDD_TX_STALL_FATAL_EVENT_THRESHOLD 2
#define EAPOL_MASK 0x8013
#define EAPOL_M1_BIT_MASK 0x8000
#define EAPOL_M2_BIT_MASK 0x0001
#define EAPOL_M3_BIT_MASK 0x8013
#define EAPOL_M4_BIT_MASK 0x0003

int gRatefromIdx[] = {
 10,20,55,100,
 10,20,55,110,
 60,90,120,180,240,360,480,540,
 65,130,195,260,390,520,585,650,
 72,144,217,289,434,578,650,722,
 65,130,195,260,390,520,585,650,
 135,270,405,540,810,1080,1215,1350,
 150,300,450,600,900,1200,1350,1500,
 135,270,405,540,810,1080,1215,1350,
 1350,1350,65,130,195,260,390, 520,
 585,650,780,1350,1350,1350,1350,1350,
 1350,1350,1350,1350,655,722,866,1350,
 1350,1350,135,270,405,540,810,1080,
 1215,1350,1350,1620,1800,1350,1350,1350,
 1350,1350,1350,1200,1350,1500,1350,1800,
 2000,1350, 292,585,877,1170,1755,2340,
 2632,2925,1350,3510,3900,1350,1350,1350,
 1350,1350,1350,1350,2925,3250,1350,3900,
 4333
 };
#ifdef FEATURE_WLAN_DIAG_SUPPORT
#define HDD_EAPOL_PACKET_TYPE_OFFSET  (15)
#define HDD_EAPOL_KEY_INFO_OFFSET     (19)
#define HDD_EAPOL_DEST_MAC_OFFSET     (0)
#define HDD_EAPOL_SRC_MAC_OFFSET      (6)
#endif /* FEATURE_WLAN_DIAG_SUPPORT */


static DEFINE_RATELIMIT_STATE(hdd_tx_timeout_rs,                 \
                              HDD_TX_TIMEOUT_RATELIMIT_INTERVAL, \
                              HDD_TX_TIMEOUT_RATELIMIT_BURST);

static struct sk_buff* hdd_mon_tx_fetch_pkt(hdd_adapter_t* pAdapter);

/*--------------------------------------------------------------------------- 
  Type declarations
  -------------------------------------------------------------------------*/ 
  
/*--------------------------------------------------------------------------- 
  Function definitions and documenation
  -------------------------------------------------------------------------*/ 

#ifdef DATA_PATH_UNIT_TEST
//Utility function to dump an sk_buff
static void dump_sk_buff(struct sk_buff * skb)
{
  VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: head = %p", __func__, skb->head);
  VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: data = %p", __func__, skb->data);
  VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: tail = %p", __func__, skb->tail);
  VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: end = %p", __func__, skb->end);
  VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: len = %d", __func__, skb->len);
  VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: data_len = %d", __func__, skb->data_len);
  VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: mac_len = %d", __func__, skb->mac_len);

  VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
     skb->data[0], skb->data[1], skb->data[2], skb->data[3], skb->data[4], 
     skb->data[5], skb->data[6], skb->data[7]); 
  VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
     skb->data[8], skb->data[9], skb->data[10], skb->data[11], skb->data[12],
     skb->data[13], skb->data[14], skb->data[15]); 
}

//Function for Unit Test only
static void transport_thread(hdd_adapter_t *pAdapter)
{
   v_U8_t staId;
   WLANTL_ACEnumType ac = WLANTL_AC_BE;
   vos_pkt_t *pVosPacket = NULL ;
   vos_pkt_t dummyPacket;
   WLANTL_MetaInfoType pktMetaInfo;
   WLANTL_RxMetaInfoType pktRxMetaInfo;
   VOS_STATUS status = VOS_STATUS_E_FAILURE;

   if (NULL == pAdapter)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter is NULL"));
       VOS_ASSERT(0);
       return;
   }

   status = hdd_tx_fetch_packet_cbk( pAdapter->pvosContext,
                                     &staId,
                                     &ac,
                                     &pVosPacket,
                                     &pktMetaInfo );
  if (status != VOS_STATUS_SUCCESS && status != VOS_STATUS_E_EMPTY)
     VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Test FAIL hdd_tx_fetch_packet_cbk", __func__);
  else
     VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Test PASS hdd_tx_fetch_packet_cbk", __func__);

  status = hdd_tx_complete_cbk(pAdapter->pvosContext, &dummyPacket, VOS_STATUS_SUCCESS);
  if (status != VOS_STATUS_SUCCESS)
     VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Test FAIL hdd_tx_complete_cbk", __func__);
  else
     VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Test PASS hdd_tx_complete_cbk", __func__);

  status = hdd_tx_low_resource_cbk(pVosPacket, pAdapter);
  if (status != VOS_STATUS_SUCCESS)
     VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Test FAIL hdd_tx_low_resource_cbk", __func__);
  else
     VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Test PASS hdd_tx_low_resource_cbk", __func__);
  
  status = hdd_rx_packet_cbk( pAdapter->pvosContext,
                              &dummyPacket,
                              staId,
                              &pktRxMetaInfo);
  if (status != VOS_STATUS_SUCCESS)
     VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Test FAIL hdd_rx_packet_cbk", __func__);
  else
     VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Test PASS hdd_rx_packet_cbk", __func__);

}
#endif


/**============================================================================
  @brief hdd_flush_tx_queues() - Utility function to flush the TX queues

  @param pAdapter : [in] pointer to adapter context  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
static VOS_STATUS hdd_flush_tx_queues( hdd_adapter_t *pAdapter )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   v_SINT_t i = -1;
   hdd_list_node_t *anchor = NULL;
   skb_list_node_t *pktNode = NULL;
   struct sk_buff *skb = NULL;

   pAdapter->isVosLowResource = VOS_FALSE;

   MTRACE(vos_trace(VOS_MODULE_ID_HDD, TRACE_CODE_HDD_FLUSH_TX_QUEUES,
                    pAdapter->sessionId, 0));

   while (++i != NUM_TX_QUEUES) 
   {
      //Free up any packets in the Tx queue
      spin_lock_bh(&pAdapter->wmm_tx_queue[i].lock);
      while (true) 
      {
         status = hdd_list_remove_front( &pAdapter->wmm_tx_queue[i], &anchor );
         if(VOS_STATUS_E_EMPTY != status)
         {
            pktNode = list_entry(anchor, skb_list_node_t, anchor);
            skb = pktNode->skb;
            //TODO
            //++pAdapter->stats.tx_dropped; 
            ++pAdapter->hdd_stats.hddTxRxStats.txFlushed;
            ++pAdapter->hdd_stats.hddTxRxStats.txFlushedAC[i];
            kfree_skb(skb);
            continue;
         }
         break;
      }
      spin_unlock_bh(&pAdapter->wmm_tx_queue[i].lock);
      // backpressure is no longer in effect
      pAdapter->isTxSuspended[i] = VOS_FALSE;
   }

   return status;
}

/**============================================================================
  @brief hdd_flush_ibss_tx_queues() - Utility function to flush the TX queues
                                      in IBSS mode

  @param pAdapter : [in] pointer to adapter context
                  : [in] Staion Id
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
void hdd_flush_ibss_tx_queues( hdd_adapter_t *pAdapter, v_U8_t STAId)
{
   v_U8_t i;
   hdd_list_node_t *anchor = NULL;
   skb_list_node_t *pktNode = NULL;
   struct sk_buff *skb = NULL;
   hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
   hdd_ibss_peer_info_t *pPeerInfo = &pHddStaCtx->ibss_peer_info;

   for (i = 0; i < NUM_TX_QUEUES; i ++)
   {
      spin_lock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[i].lock);
      while (true)
      {
         if (VOS_STATUS_E_EMPTY !=
              hdd_list_remove_front(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[i],
                                    &anchor))
         {
            //If success then we got a valid packet from some AC
            pktNode = list_entry(anchor, skb_list_node_t, anchor);
            skb = pktNode->skb;
            ++pAdapter->stats.tx_dropped;
            ++pAdapter->hdd_stats.hddTxRxStats.txFlushed;
            ++pAdapter->hdd_stats.hddTxRxStats.txFlushedAC[i];
            kfree_skb(skb);
            continue;
         }
         break;
      }
      spin_unlock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[i].lock);
   }
}

static struct sk_buff* hdd_mon_tx_fetch_pkt(hdd_adapter_t* pAdapter)
{
   skb_list_node_t *pktNode = NULL;
   struct sk_buff *skb = NULL;
   v_SIZE_t size = 0;
   WLANTL_ACEnumType ac = 0;
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   hdd_list_node_t *anchor = NULL;

   if (NULL == pAdapter)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
       FL("pAdapter is NULL"));
      VOS_ASSERT(0);
      return NULL;
   }

   // do we have any packets pending in this AC?
   hdd_list_size( &pAdapter->wmm_tx_queue[ac], &size ); 
   if( size == 0 )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 "%s: NO Packet Pending", __func__);
      return NULL;
   }

   //Remove the packet from the queue
   spin_lock_bh(&pAdapter->wmm_tx_queue[ac].lock);
   status = hdd_list_remove_front( &pAdapter->wmm_tx_queue[ac], &anchor );
   spin_unlock_bh(&pAdapter->wmm_tx_queue[ac].lock);

   if(VOS_STATUS_SUCCESS == status)
   {
      //If success then we got a valid packet from some AC
      pktNode = list_entry(anchor, skb_list_node_t, anchor);
      skb = pktNode->skb;
   }
   else
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to remove Packet from the list",
                  __func__);

      return NULL;
   }

   // if we are in a backpressure situation see if we can turn the hose back on
   if ( (pAdapter->isTxSuspended[ac]) &&
        (size <= HDD_TX_QUEUE_LOW_WATER_MARK) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,
                 "%s: TX queue[%d] re-enabled", __func__, ac);
      pAdapter->isTxSuspended[ac] = VOS_FALSE;      
      /* Enable Queues which we have disabled earlier */
      hddLog(VOS_TRACE_LEVEL_INFO, FL("Enabling queues"));
      netif_tx_start_all_queues( pAdapter->dev ); 
   }

   return skb;
}

void hdd_mon_tx_mgmt_pkt(hdd_adapter_t* pAdapter)
{
   hdd_cfg80211_state_t *cfgState;
   struct sk_buff* skb;
   hdd_adapter_t* pMonAdapter = NULL;
   struct ieee80211_hdr *hdr;
   hdd_context_t *pHddCtx;
   int ret = 0;

   ENTER();
   if (pAdapter == NULL)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
       FL("pAdapter is NULL"));
      VOS_ASSERT(0);
      return;
   }
   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   ret = wlan_hdd_validate_context(pHddCtx);
   if (0 != ret)
   {
       return;
   }
   pMonAdapter = hdd_get_adapter( pAdapter->pHddCtx, WLAN_HDD_MONITOR );
   if (pMonAdapter == NULL)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: pMonAdapter is NULL", __func__);
       return;
   }
   cfgState = WLAN_HDD_GET_CFG_STATE_PTR( pAdapter );

   if( NULL != cfgState->buf )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
          "%s: Already one MGMT packet Tx going on", __func__);
      return;
   }

   skb = hdd_mon_tx_fetch_pkt(pMonAdapter);

   if (NULL == skb)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
       "%s: No Packet Pending", __func__);
      return;
   }

   cfgState->buf = vos_mem_malloc( skb->len ); //buf;
   if( cfgState->buf == NULL )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
          "%s: Failed to Allocate memory", __func__);
      goto fail;
   }

   cfgState->len = skb->len;

   vos_mem_copy( cfgState->buf, skb->data, skb->len);

   cfgState->skb = skb; //buf;
   cfgState->action_cookie = (uintptr_t)cfgState->buf;

   hdr = (struct ieee80211_hdr *)skb->data;
   if( (hdr->frame_control & HDD_FRAME_TYPE_MASK)
                                       == HDD_FRAME_TYPE_MGMT )
   {
       if( (hdr->frame_control & HDD_FRAME_SUBTYPE_MASK)
                                       == HDD_FRAME_SUBTYPE_DEAUTH )
       {
          struct tagCsrDelStaParams delStaParams;

          WLANSAP_PopulateDelStaParams(hdr->addr1,
                                  eSIR_MAC_DEAUTH_LEAVING_BSS_REASON,
                                 (SIR_MAC_MGMT_DEAUTH >> 4), &delStaParams);

          hdd_softap_sta_deauth(pAdapter, &delStaParams);
          goto mgmt_handled;
       }
       else if( (hdr->frame_control & HDD_FRAME_SUBTYPE_MASK) 
                                      == HDD_FRAME_SUBTYPE_DISASSOC )
       {
          hdd_softap_sta_disassoc( pAdapter, hdr->addr1 ); 
          goto mgmt_handled;
       }
   }
   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
      "%s: Sending action frame to SAP to TX, Len %d", __func__, skb->len);

   if (VOS_STATUS_SUCCESS != 
      WLANSAP_SendAction( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                           skb->data, skb->len, 0) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
          "%s: WLANSAP_SendAction returned fail", __func__);
      hdd_sendActionCnf( pAdapter, FALSE );
   }
   EXIT();
   return;

mgmt_handled:
   EXIT();
   hdd_sendActionCnf( pAdapter, TRUE );
   return;
fail:
   kfree_skb(pAdapter->skb_to_tx);
   pAdapter->skb_to_tx = NULL;
   return;
}

void __hdd_mon_tx_work_queue(struct work_struct *work)
{
   hdd_adapter_t* pAdapter = container_of(work, hdd_adapter_t, monTxWorkQueue);
   hdd_mon_tx_mgmt_pkt(pAdapter);
}

void hdd_mon_tx_work_queue(struct work_struct *work)
{
   vos_ssr_protect(__func__);
   __hdd_mon_tx_work_queue(work);
   vos_ssr_unprotect(__func__);
}

int hdd_mon_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
           "%s: Packet Rcvd at Monitor interface,"
             " Dropping the packet",__func__);
   kfree_skb(skb);
   return NETDEV_TX_OK;
}

/**============================================================================
  @brief hdd_dhcp_pkt_info() -
               Function to log DHCP pkt info

  @param skb      : [in]  pointer to OS packet (sk_buff)
  @return         : None
  ===========================================================================*/

void hdd_dhcp_pkt_info(struct sk_buff *skb)
{
    /* port no 67 (0x43) or 68 (0x44) */

    if (*((u8*)skb->data + BOOTP_MSG_OFFSET) == BOOTP_REQUEST_MSG)
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Request"));
    else if (*((u8*)skb->data + BOOTP_MSG_OFFSET) == BOOTP_RESPONSE_MSG)
        hddLog(VOS_TRACE_LEVEL_INFO, FL("Response"));
    else
        hddLog(VOS_TRACE_LEVEL_INFO, FL("DHCP invalid"));

    hddLog(VOS_TRACE_LEVEL_INFO,
            FL("DHCP Dest Addr: %pM Src Addr %pM "
                " source port : %d, dest port : %d"),
            skb->data, (skb->data + 6),
            ntohs(*((u16*)((u8*)skb->data + UDP_SRC_PORT_OFFSET))),
            ntohs(*((u16*)((u8*)skb->data + UDP_DEST_PORT_OFFSET))));

    if ((skb->data[DHCP_OPTION53_OFFSET] == DHCP_OPTION53) &&
        (skb->data[DHCP_OPTION53_LENGTH_OFFSET] == DHCP_OPTION53_LENGTH)) {

        switch (skb->data[DHCP_OPTION53_STATUS_OFFSET]) {
        case DHCPDISCOVER:
            hddLog(VOS_TRACE_LEVEL_INFO,FL("DHCP DISCOVER"));
            break;
        case DHCPREQUEST:
            hddLog(VOS_TRACE_LEVEL_INFO,FL("DHCP REQUEST"));
            break;
        case DHCPOFFER:
            hddLog(VOS_TRACE_LEVEL_INFO,FL("DHCP OFFER"));
            break;
        case DHCPACK:
            hddLog(VOS_TRACE_LEVEL_INFO,FL("DHCP ACK"));
            break;
        case DHCPNAK:
            hddLog(VOS_TRACE_LEVEL_INFO,FL("DHCP NACK"));
            break;
        case DHCPRELEASE:
            hddLog(VOS_TRACE_LEVEL_INFO,FL("DHCP RELEASE"));
            break;
        case DHCPINFORM:
            hddLog(VOS_TRACE_LEVEL_INFO,FL("DHCP INFORM"));
            break;

        default:
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "%s: DHCP Not Defined OPTION53 : %d", __func__,
                    skb->data[DHCP_OPTION53_STATUS_OFFSET]);
        }
    }
}

/**============================================================================
  @brief hdd_dump_dhcp_pkt() -
               Function to dump DHCP packets in TX and RX path.

  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param path     : [in]  bool indicating TX/RX path
  @return         : None
  ===========================================================================*/
void hdd_dump_dhcp_pkt(struct sk_buff *skb, int path)
{

    if ((ntohs(*((u16*)((u8*)skb->data + ETH_TYPE_OFFSET)))
                == ETH_TYPE_IP_PKT) ||
            (ntohs(*((u8*)skb->data + PROTOCOL_OFFSET)) == UDP_PROTOCOL)) {

        /* IP protocol 12 bytes of mac addresses in 802.3 header */
        if ( ntohs(*((u16*)((u8*)skb->data + UDP_DEST_PORT_OFFSET))) ==
                BOOTP_SERVER_PORT  ||
                ntohs(*((u16*)((u8*)skb->data + UDP_DEST_PORT_OFFSET))) ==
                BOOTP_CLIENT_PORT  ||
                ntohs(*((u16*)((u8*)skb->data + UDP_SRC_PORT_OFFSET))) ==
                BOOTP_SERVER_PORT  ||
                ntohs(*((u16*)((u8*)skb->data + UDP_SRC_PORT_OFFSET))) ==
                BOOTP_CLIENT_PORT ) {

            if (path == TX_PATH) {
                hddLog(VOS_TRACE_LEVEL_INFO, FL("DHCP TX PATH"));
            } else {
                hddLog(VOS_TRACE_LEVEL_INFO, FL("DHCP RX PATH"));
            }

            hdd_dhcp_pkt_info(skb);
        }
    }
}

/**============================================================================
   @brief hdd_ibss_hard_start_xmit() - Function registered with the Linux OS for
   transmitting packets in case of IBSS. There are 2 versions of this function.
   One that uses locked queue and other that uses lockless queues. Both have been
   retained to do some performance testing

   @param skb      : [in]  pointer to OS packet (sk_buff)
   @param dev      : [in] pointer to network device

   @return         : NET_XMIT_DROP if packets are dropped
                   : NET_XMIT_SUCCESS if packet is enqueued succesfully
   ===========================================================================*/
 int hdd_ibss_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
 {
    VOS_STATUS status;
    WLANTL_ACEnumType ac;
    sme_QosWmmUpType up;
    skb_list_node_t *pktNode = NULL;
    hdd_list_node_t *anchor = NULL;
    v_SIZE_t pktListSize = 0;
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    hdd_ibss_peer_info_t * pPeerInfo;
    v_U8_t STAId = WLAN_MAX_STA_COUNT;
    v_BOOL_t txSuspended = VOS_FALSE;
    struct sk_buff *skb1;
    v_MACADDR_t *pDestMacAddress = (v_MACADDR_t*)skb->data;

    if (NULL == pHddCtx || NULL == pHddStaCtx) {
        VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                   "%s HDD context is NULL", __func__);
        return NETDEV_TX_BUSY;
    }
    pPeerInfo = &pHddStaCtx->ibss_peer_info;
    ++pAdapter->hdd_stats.hddTxRxStats.txXmitCalled;

    //Get TL AC corresponding to Qdisc queue index/AC.
    ac = hdd_QdiscAcToTlAC[skb->queue_mapping];

    if (eConnectionState_IbssDisconnected == pHddStaCtx->conn_info.connState)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                  "%s: Tx frame in disconnected state in IBSS mode", __func__);
        ++pAdapter->stats.tx_dropped;
        ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
        ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[ac];
        kfree_skb(skb);
        return NETDEV_TX_OK;
    }

     STAId = hdd_sta_id_find_from_mac_addr(pAdapter, pDestMacAddress);
     if ((STAId == HDD_WLAN_INVALID_STA_ID) &&
           (vos_is_macaddr_broadcast( pDestMacAddress ) ||
            vos_is_macaddr_group(pDestMacAddress)))
     {
          STAId = IBSS_BROADCAST_STAID;
          VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_LOW,
                     "%s: BC/MC packet", __func__);
     }
     else if (STAId >= HDD_MAX_NUM_IBSS_STA)
     {
          VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                     "%s: Received Unicast frame with invalid staID", __func__);
          ++pAdapter->stats.tx_dropped;
          ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
          ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[ac];
          kfree_skb(skb);
          return NETDEV_TX_OK;
     }

    //user priority from IP header, which is already extracted and set from
    //select_queue call back function
    up = skb->priority;
    ++pAdapter->hdd_stats.hddTxRxStats.txXmitClassifiedAC[ac];

    VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
               "%s: Classified as ac %d up %d", __func__, ac, up);

    if ( pHddCtx->cfg_ini->gEnableDebugLog & VOS_PKT_PROTO_TYPE_DHCP )
    {
       hdd_dump_dhcp_pkt(skb, TX_PATH);
    }

     spin_lock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].lock);
     hdd_list_size(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac], &pktListSize);
     if(pktListSize >= pAdapter->aTxQueueLimit[ac])
     {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
             "%s: station %d ac %d queue over limit %d", __func__, STAId, ac, pktListSize);
        pPeerInfo->ibssStaInfo[STAId].txSuspended[ac] = VOS_TRUE;
        netif_stop_subqueue(dev, skb_get_queue_mapping(skb));
        txSuspended = VOS_TRUE;
        spin_unlock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].lock);
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                   "%s: TX queue full for AC=%d Disable OS TX queue",
                   __func__, ac );
         return NETDEV_TX_BUSY;
     }

     /* If 3/4th of the max queue size is used then enable the flag.
      * This flag indicates to place the DHCP packets in VOICE AC queue.*/
    if (WLANTL_AC_BE == ac)
    {
       if (pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].count >= HDD_TX_QUEUE_LOW_WATER_MARK)
       {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                      "%s: TX queue for Best Effort AC is 3/4th full", __func__);
           pAdapter->isVosLowResource = VOS_TRUE;
       }
       else
       {
           pAdapter->isVosLowResource = VOS_FALSE;
       }
    }

    spin_unlock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].lock);

    //Use the skb->cb field to hold the list node information
    pktNode = (skb_list_node_t *)&skb->cb;

    //Stick the OS packet inside this node.
    pktNode->skb = skb;

    //Stick the User Priority inside this node
    pktNode->userPriority = up;

    INIT_LIST_HEAD(&pktNode->anchor);

    spin_lock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].lock);
    status = hdd_list_insert_back_size( &pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac],
                                       &pktNode->anchor, &pktListSize );
    spin_unlock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].lock);
    if ( !VOS_IS_STATUS_SUCCESS( status ) )
    {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                  "%s:Insert Tx queue failed. Pkt dropped", __func__);
       ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
       ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[ac];
       ++pAdapter->stats.tx_dropped;
       kfree_skb(skb);
       return NETDEV_TX_OK;
    }

    ++pAdapter->hdd_stats.hddTxRxStats.txXmitQueued;
    ++pAdapter->hdd_stats.hddTxRxStats.txXmitQueuedAC[ac];
    ++pAdapter->hdd_stats.hddTxRxStats.pkt_tx_count;

    if (1 == pktListSize)
    {
       //Let TL know we have a packet to send for this AC
       status = WLANTL_STAPktPending( pHddCtx->pvosContext, STAId, ac );

       if ( !VOS_IS_STATUS_SUCCESS( status ) )
       {
          VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                   "%s: Failed to signal TL for AC=%d STAId =%d",
                       __func__, ac, STAId );

          /* Remove the packet from queue. It must be at the back of the queue, as TX thread
           * cannot preempt us in the middle as we are in a soft irq context.
           *  Also it must be the same packet that we just allocated.
           */
          spin_lock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].lock);
          status = hdd_list_remove_back( &pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac], &anchor);
          spin_unlock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].lock);
          /* Free the skb only if we are able to remove it from the list.
           * If we are not able to retrieve it from the list it means that
           * the skb was pulled by TX Thread and is use so we should not free
           * it here
           */
          if (VOS_IS_STATUS_SUCCESS(status))
          {
             pktNode = list_entry(anchor, skb_list_node_t, anchor);
             skb1 = pktNode->skb;
             kfree_skb(skb1);
          }
          ++pAdapter->stats.tx_dropped;
          ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
          ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[ac];
          return NETDEV_TX_OK;
       }
    }

    dev->trans_start = jiffies;

    return NETDEV_TX_OK;
 }

/**============================================================================
  @brief hdd_hard_start_xmit() - Function registered with the Linux OS for
  transmitting packets. There are 2 versions of this function. One that uses
  locked queue and other that uses lockless queues. Both have been retained to
  do some performance testing

  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param dev      : [in] pointer to Libra network device

  @return         : NET_XMIT_DROP if packets are dropped
                  : NET_XMIT_SUCCESS if packet is enqueued succesfully
  ===========================================================================*/
int hdd_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
   VOS_STATUS status;
   WLANTL_ACEnumType qid, ac;
   sme_QosWmmUpType up;
   skb_list_node_t *pktNode = NULL;
   hdd_list_node_t *anchor = NULL;
   v_SIZE_t pktListSize = 0;
   hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
   v_BOOL_t granted;
   v_U8_t STAId = WLAN_MAX_STA_COUNT;
   hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   v_BOOL_t txSuspended = VOS_FALSE;
   struct sk_buff *skb1;

   if (NULL == pHddCtx) {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                  "%s HDD context is NULL", __func__);
       return NETDEV_TX_BUSY;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.txXmitCalled;

   if (unlikely(netif_subqueue_stopped(dev, skb))) {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                  "%s is called when netif TX %d is disabled",
                  __func__, skb->queue_mapping);
       return NETDEV_TX_BUSY;
   }

   //Get TL Q index corresponding to Qdisc queue index/AC.
   qid = hdd_QdiscAcToTlAC[skb->queue_mapping];
   ac  = qid;

   if (qid == WLANTL_AC_HIGH_PRIO)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s It must be a Eapol/Wapi/DHCP packet device_mode:%d",
                __func__, pAdapter->device_mode);
      ac = hddWmmUpToAcMap[skb->priority];
   }

      if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
      {
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                FL("Tx frame in not associated state in %d context"),
                    pAdapter->device_mode);
         ++pAdapter->stats.tx_dropped;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[qid];
         kfree_skb(skb);
         return NETDEV_TX_OK;
      }
      STAId = pHddStaCtx->conn_info.staId[0];

   //user priority from IP header, which is already extracted and set from
   //select_queue call back function
   up = skb->priority;

   ++pAdapter->hdd_stats.hddTxRxStats.txXmitClassifiedAC[qid];

#ifdef HDD_WMM_DEBUG
   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
              "%s: Classified as ac %d up %d", __func__, ac, up);
#endif // HDD_WMM_DEBUG

   if (pHddCtx->cfg_ini->gEnableRoamDelayStats)
   {
       vos_record_roam_event(e_HDD_FIRST_XMIT_TIME, (void *)skb, 0);
   }

   spin_lock(&pAdapter->wmm_tx_queue[qid].lock);
   /*CR 463598,384996*/
   /*For every increment of 10 pkts in the queue, we inform TL about pending pkts.
    *We check for +1 in the logic,to take care of Zero count which
    *occurs very frequently in low traffic cases */
   if((pAdapter->wmm_tx_queue[qid].count + 1) % 10 == 0)
   {
      /* Use the following debug statement during Engineering Debugging.There are chance that this will lead to a Watchdog Bark
            * if it is in the mainline code and if the log level is enabled by someone for debugging
           VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,"%s:Queue is Filling up.Inform TL again about pending packets", __func__);*/

      status = WLANTL_STAPktPending( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                    STAId, qid
                                    );
      if ( !VOS_IS_STATUS_SUCCESS( status ) )
      {
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                    "%s: WLANTL_STAPktPending() returned error code %d",
                    __func__, status);
         ++pAdapter->stats.tx_dropped;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[qid];
         kfree_skb(skb);
         spin_unlock(&pAdapter->wmm_tx_queue[qid].lock);
         return NETDEV_TX_OK;
      }
   }
   //If we have already reached the max queue size, disable the TX queue
   if ( pAdapter->wmm_tx_queue[qid].count == pAdapter->wmm_tx_queue[qid].max_size)
   {
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitBackPressured;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitBackPressuredAC[qid];
         hddLog(VOS_TRACE_LEVEL_INFO, FL("Disabling queue for QId %d"), qid);
         netif_tx_stop_queue(netdev_get_tx_queue(dev, skb_get_queue_mapping(skb)));
         pAdapter->isTxSuspended[qid] = VOS_TRUE;
         txSuspended = VOS_TRUE;
         MTRACE(vos_trace(VOS_MODULE_ID_HDD, TRACE_CODE_HDD_STOP_NETDEV,
                          pAdapter->sessionId, qid));
   }

   /* If 3/4th of the max queue size is used then enable the flag.
    * This flag indicates to place the DHCP packets in VOICE AC queue.*/
   if (WLANTL_AC_BE == qid)
   {
      if (pAdapter->wmm_tx_queue[qid].count >= HDD_TX_QUEUE_LOW_WATER_MARK)
      {
          pAdapter->isVosLowResource = VOS_TRUE;
      }
      else
      {
          pAdapter->isVosLowResource = VOS_FALSE;
      }
   }

   spin_unlock(&pAdapter->wmm_tx_queue[qid].lock);

   if (( NULL != pHddCtx ) &&
      (pHddCtx->cfg_ini->gEnableDebugLog & VOS_PKT_PROTO_TYPE_DHCP))
   {
       hdd_dump_dhcp_pkt(skb, TX_PATH);
   }

   if (VOS_TRUE == txSuspended)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                  "%s: TX queue full for QId=%d Disable OS TX queue",
                  __func__, qid );
      return NETDEV_TX_BUSY;
   }

   //Use the skb->cb field to hold the list node information
   pktNode = (skb_list_node_t *)&skb->cb;

   //Stick the OS packet inside this node.
   pktNode->skb = skb;

   //Stick the User Priority inside this node
   pktNode->userPriority = up;


   INIT_LIST_HEAD(&pktNode->anchor);

   //Insert the OS packet into the appropriate AC queue
   spin_lock(&pAdapter->wmm_tx_queue[qid].lock);
   status = hdd_list_insert_back_size( &pAdapter->wmm_tx_queue[qid], &pktNode->anchor, &pktListSize );
   spin_unlock(&pAdapter->wmm_tx_queue[qid].lock);

   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,"%s:Insert Tx queue failed. Pkt dropped", __func__);
      ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
      ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[qid];
      ++pAdapter->stats.tx_dropped;
      kfree_skb(skb);
      return NETDEV_TX_OK;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.txXmitQueued;
   ++pAdapter->hdd_stats.hddTxRxStats.txXmitQueuedAC[qid];
   ++pAdapter->hdd_stats.hddTxRxStats.pkt_tx_count;

   if (HDD_PSB_CHANGED == pAdapter->psbChanged)
   {
      /* Function which will determine acquire admittance for a
       * WMM AC is required or not based on psb configuration done
       * in the framework
       */
       hdd_wmm_acquire_access_required(pAdapter, ac);
   }

   //Make sure we have access to this access category
   if (((pAdapter->psbChanged & (1 << ac)) && likely(pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessAllowed)) ||
           (pHddStaCtx->conn_info.uIsAuthenticated == VOS_FALSE))
   {
      granted = VOS_TRUE;
   }
   else
   {
      status = hdd_wmm_acquire_access( pAdapter, ac, &granted );
      pAdapter->psbChanged |= (1 << ac);
   }

   if ( (granted && ( pktListSize == 1 )) ||
        (qid == WLANTL_AC_HIGH_PRIO))
   {
      //Let TL know we have a packet to send for this AC
      //VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s:Indicating Packet to TL", __func__);
      status = WLANTL_STAPktPending(
                                  (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                   STAId, qid );
      if ( !VOS_IS_STATUS_SUCCESS( status ) )
      {
         VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,
                   "%s: Failed to signal TL for QId=%d", __func__, qid );

         //Remove the packet from queue. It must be at the back of the queue, as TX thread cannot preempt us in the middle
         //as we are in a soft irq context. Also it must be the same packet that we just allocated.
         spin_lock(&pAdapter->wmm_tx_queue[qid].lock);
         status = hdd_list_remove_back( &pAdapter->wmm_tx_queue[qid], &anchor );
         spin_unlock(&pAdapter->wmm_tx_queue[qid].lock);
         /* Free the skb only if we are able to remove it from the list.
          * If we are not able to retrieve it from the list it means that
          * the skb was pulled by TX Thread and is use so we should not free
          * it here
          */
         if (VOS_IS_STATUS_SUCCESS(status))
         {
            pktNode = list_entry(anchor, skb_list_node_t, anchor);
            skb1 = pktNode->skb;
            kfree_skb(skb1);
         }
         ++pAdapter->stats.tx_dropped;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[qid];
         return NETDEV_TX_OK;
      }
   }

   dev->trans_start = jiffies;

   return NETDEV_TX_OK;
}

/**============================================================================
  @brief hdd_Ibss_GetStaId() - Get the StationID using the Peer Mac address

  @param pHddStaCtx : [in] pointer to HDD Station Context
  pMacAddress [in]  pointer to Peer Mac address
  staID [out]  pointer to Station Index
  @return    : VOS_STATUS_SUCCESS/VOS_STATUS_E_FAILURE
  ===========================================================================*/

VOS_STATUS hdd_Ibss_GetStaId(hdd_station_ctx_t *pHddStaCtx, v_MACADDR_t *pMacAddress, v_U8_t *staId)
{
    v_U8_t idx;

    for (idx = 0; idx < HDD_MAX_NUM_IBSS_STA; idx++)
    {
        if (vos_mem_compare(&pHddStaCtx->conn_info.peerMacAddress[ idx ],
                pMacAddress, sizeof(v_MACADDR_t)))
        {
            *staId = pHddStaCtx->conn_info.staId[idx];
            return VOS_STATUS_SUCCESS;
        }
    }

    return VOS_STATUS_E_FAILURE;
}

/**============================================================================
  @brief __hdd_tx_timeout() - Function handles timeout during transmission.

  @param dev : [in] pointer to network device
  @return    : None
  ===========================================================================*/
void __hdd_tx_timeout(struct net_device *dev)
{
   hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;
   struct netdev_queue *txq;
   hdd_remain_on_chan_ctx_t *pRemainChanCtx;
   int i = 0;
   int status = 0;
   v_ULONG_t diff_in_jiffies = 0;
   hdd_station_ctx_t *pHddStaCtx = NULL;

   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
      "%s: Transmission timeout occurred jiffies %lu dev->trans_start %lu",
        __func__,jiffies,dev->trans_start);

   if ( NULL == pAdapter )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter is NULL"));
      VOS_ASSERT(0);
      return;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   status = wlan_hdd_validate_context(pHddCtx);
   if (status !=0 )
   {
       return;
   }

   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   if ( NULL == pHddStaCtx )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pHddStaCtx is NULL"));
      return;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.txTimeoutCount;

   //Getting here implies we disabled the TX queues for too long. Queues are 
   //disabled either because of disassociation or low resource scenarios. In
   //case of disassociation it is ok to ignore this. But if associated, we have
   //do possible recovery here

   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              "num_bytes AC0: %d AC1: %d AC2: %d AC3: %d",
              pAdapter->wmm_tx_queue[0].count,
              pAdapter->wmm_tx_queue[1].count,
              pAdapter->wmm_tx_queue[2].count,
              pAdapter->wmm_tx_queue[3].count);

   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              "tx_suspend AC0: %d AC1: %d AC2: %d AC3: %d",
              pAdapter->isTxSuspended[0],
              pAdapter->isTxSuspended[1],
              pAdapter->isTxSuspended[2],
              pAdapter->isTxSuspended[3]);

   for (i = 0; i < dev->num_tx_queues; i++)
   {
      txq = netdev_get_tx_queue(dev, i);
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "Queue%d status: %d txq->trans_start %lu",
                 i, netif_tx_queue_stopped(txq),txq->trans_start);
   }

   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              "carrier state: %d", netif_carrier_ok(dev));

   /* continuousTxTimeoutCount will be reset whenever TL fetches packet
    * from HDD
    */
   ++pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount;

   diff_in_jiffies = jiffies - pAdapter->hdd_stats.hddTxRxStats.jiffiesLastTxTimeOut;
   if((pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount > 1)&&
     ((diff_in_jiffies) > (HDD_TX_TIMEOUT * 2 ))
     )
   {
        /*
         * In Open security case when there is no traffic is running, it may possible
         * tx time-out may once happen and later we recovered then we need to
         * reset the continuousTxTimeoutCount because it is only getting modified
         * when traffic is running. So if over a period of time if this count reaches
         * to HDD_TX_STALL_SSR_THRESHOLD  then host is triggering false subsystem restart.
         * so in genuine Tx Time out case kernel will call the tx time-out back to back at
         * interval of HDD_TX_TIMEOUT.So now we are checking if previous TX TIME out was
         * occurred more then twice of HDD_TX_TIMEOUT back then we may recovered here.
        */
        pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount = 0;
        VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                  FL("This is false alarm so resetting the continuousTxTimeoutCount"));
   }

   //update last jiffies after the check
   pAdapter->hdd_stats.hddTxRxStats.jiffiesLastTxTimeOut = jiffies;

   if (!pHddStaCtx->conn_info.uIsAuthenticated) {
      VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                FL("TL is not in authenticated state so skipping SSR"));
      pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount = 0;
      goto print_log;
   }
   if (pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount ==
          HDD_TX_STALL_KICKDXE_THRESHOLD)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Request Kick DXE for recovery",__func__);
      WLANTL_TLDebugMessage(WLANTL_DEBUG_KICKDXE);
   }
   if (pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount ==
          HDD_TX_STALL_RECOVERY_THRESHOLD)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Request firmware for recovery",__func__);
      WLANTL_TLDebugMessage(WLANTL_DEBUG_FW_CLEANUP);
   }
   /*
    * This function is getting called in softirq context, So don't hold
    * any mutex.
    * There is no harm here in not holding the mutex as long as we are
    * not accessing the pRemainChanCtx contents.
    */
   pRemainChanCtx = hdd_get_remain_on_channel_ctx(pHddCtx);
   if (!pRemainChanCtx)
   {
      if (pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount >
          HDD_TX_STALL_SSR_THRESHOLD)
      {
          // Driver could not recover, issue SSR
          VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                    "%s: Cannot recover from Data stall Issue SSR",
                      __func__);
          WLANTL_FatalError();
          return;
      }
   }
   else
   {
       mutex_unlock(&pHddCtx->roc_lock);
      VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "Remain on channel in progress");
      /* The supplicant can retry "P2P Invitation Request" for 120 times
       * and so there is a possbility that we can remain off channel for
       * the entire duration of these retries(which can be max 60sec).
       * If we encounter such a case, let us not trigger SSR after 30sec
       * but wait for 60sec to let the device go on home channel and start
       * tx. If tx does not start within 70sec we will issue SSR.
       */
      if (pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount >
          HDD_TX_STALL_SSR_THRESHOLD_HIGH)
      {
          // Driver could not recover, issue SSR
          VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                    "%s: Cannot recover from Data stall Issue SSR",
                      __func__);
          WLANTL_FatalError();
          return;
      }
   }

print_log:
   /* If Tx stalled for a long time then *hdd_tx_timeout* is called
    * every 5sec. The TL debug spits out a lot of information on the
    * serial console, if it is called every time *hdd_tx_timeout* is
    * called then we may get a watchdog bite on the Application
    * processor, so ratelimit the TL debug logs.
    */
   if (__ratelimit(&hdd_tx_timeout_rs))
   {
      hdd_wmm_tx_snapshot(pAdapter);
      WLANTL_TLDebugMessage(WLANTL_DEBUG_TX_SNAPSHOT);
   }

}

/**============================================================================
  @brief hdd_tx_timeout() - Function called by OS if there is any
  timeout during transmission. Since HDD simply enqueues packet
  and returns control to OS right away, this would never be invoked

  @param dev : [in] pointer to network device
  @return    : None
  ===========================================================================*/
void hdd_tx_timeout(struct net_device *dev)
{
    vos_ssr_protect(__func__);
    __hdd_tx_timeout(dev);
    vos_ssr_unprotect(__func__);
}

/**============================================================================
  @brief __hdd_stats() - Function registered with the Linux OS for
  device TX/RX statistic

  @param dev      : [in] pointer to Libra network device
  
  @return         : pointer to net_device_stats structure
  ===========================================================================*/
struct net_device_stats* __hdd_stats(struct net_device *dev)
{
   hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);

   if ( NULL == pAdapter )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter is NULL"));
      VOS_ASSERT(0);
      return NULL;
   }
   
   return &pAdapter->stats;
}

struct net_device_stats* hdd_stats(struct net_device *dev)
{
    struct net_device_stats* dev_stats;

    vos_ssr_protect(__func__);
    dev_stats = __hdd_stats(dev);
    vos_ssr_unprotect(__func__);

    return dev_stats;
}

/**============================================================================
  @brief hdd_ibss_init_tx_rx() - Init function to initialize Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
void hdd_ibss_init_tx_rx( hdd_adapter_t *pAdapter )
{
   v_U8_t i;
   v_U8_t STAId = 0;
   hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
   hdd_ibss_peer_info_t *pPeerInfo = &pHddStaCtx->ibss_peer_info;
   v_U8_t pACWeights[] = {
                           HDD_SOFTAP_BK_WEIGHT_DEFAULT,
                           HDD_SOFTAP_BE_WEIGHT_DEFAULT,
                           HDD_SOFTAP_VI_WEIGHT_DEFAULT,
                           HDD_SOFTAP_VO_WEIGHT_DEFAULT
                         };

   pAdapter->isVosOutOfResource = VOS_FALSE;
   pAdapter->isVosLowResource = VOS_FALSE;

   // Since SAP model is used for IBSS also. Using same queue length as in SAP.
   pAdapter->aTxQueueLimit[WLANTL_AC_BK] = HDD_SOFTAP_TX_BK_QUEUE_MAX_LEN;
   pAdapter->aTxQueueLimit[WLANTL_AC_BE] = HDD_SOFTAP_TX_BE_QUEUE_MAX_LEN;
   pAdapter->aTxQueueLimit[WLANTL_AC_VI] = HDD_SOFTAP_TX_VI_QUEUE_MAX_LEN;
   pAdapter->aTxQueueLimit[WLANTL_AC_VO] = HDD_SOFTAP_TX_VO_QUEUE_MAX_LEN;

   for (STAId = 0; STAId < HDD_MAX_NUM_IBSS_STA; STAId++)
   {
      vos_mem_zero(&pPeerInfo->ibssStaInfo[STAId], sizeof(hdd_ibss_station_info_t));
      for (i = 0; i < NUM_TX_QUEUES; i ++)
      {
         hdd_list_init(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[i], HDD_TX_QUEUE_MAX_LEN);
      }
   }

   /* Update the AC weights suitable for SoftAP mode of operation */
   WLANTL_SetACWeights((WLAN_HDD_GET_CTX(pAdapter))->pvosContext, pACWeights);
}

/**============================================================================
  @brief hdd_ibss_deinit_tx_rx() - Deinit function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context..
  @return         : VOS_STATUS_E_FAILURE if any errors encountered.
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_ibss_deinit_tx_rx( hdd_adapter_t *pAdapter )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   v_U8_t STAId = 0;
   hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
   hdd_ibss_peer_info_t * pPeerInfo = &pHddStaCtx->ibss_peer_info;
   hdd_list_node_t *anchor = NULL;
   skb_list_node_t *pktNode = NULL;
   struct sk_buff *skb = NULL;
   v_SINT_t i = -1;

   for (STAId = 0; STAId < HDD_MAX_NUM_IBSS_STA; STAId++)
   {
      if (VOS_FALSE == pPeerInfo->ibssStaInfo[STAId].isUsed)
      {
         continue;
      }
      for (i = 0; i < NUM_TX_QUEUES; i ++)
      {
         spin_lock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[i].lock);
         while (true)
         {
            status = hdd_list_remove_front ( &pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[i], &anchor);

            if (VOS_STATUS_E_EMPTY != status)
            {
               //If success then we got a valid packet from some AC
               pktNode = list_entry(anchor, skb_list_node_t, anchor);
               skb = pktNode->skb;
               ++pAdapter->stats.tx_dropped;
               ++pAdapter->hdd_stats.hddTxRxStats.txFlushed;
               ++pAdapter->hdd_stats.hddTxRxStats.txFlushedAC[i];
               kfree_skb(skb);
               continue;
            }

            //current list is empty
            break;
         }
         pPeerInfo->ibssStaInfo[STAId].txSuspended[i] = VOS_FALSE;
         spin_unlock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[i].lock);
      }
   }
   pAdapter->isVosLowResource = VOS_FALSE;

   return status;
}

/**============================================================================
  @brief hdd_init_tx_rx() - Init function to initialize Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_init_tx_rx( hdd_adapter_t *pAdapter )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   v_SINT_t i = -1;

   if ( NULL == pAdapter )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter is NULL"));
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   pAdapter->isVosOutOfResource = VOS_FALSE;
   pAdapter->isVosLowResource = VOS_FALSE;

   //vos_mem_zero(&pAdapter->stats, sizeof(struct net_device_stats));
   //Will be zeroed out during alloc

   while (++i != NUM_TX_QUEUES)
   { 
      pAdapter->isTxSuspended[i] = VOS_FALSE; 
      hdd_list_init( &pAdapter->wmm_tx_queue[i], HDD_TX_QUEUE_MAX_LEN);
   }

   return status;
}


/**============================================================================
  @brief hdd_deinit_tx_rx() - Deinit function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_deinit_tx_rx( hdd_adapter_t *pAdapter )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   v_SINT_t i = -1;

   if ( NULL == pAdapter )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter is NULL"));
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   status = hdd_flush_tx_queues(pAdapter);
   if (VOS_STATUS_SUCCESS != status)
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,
          FL("failed to flush tx queues"));

   while (++i != NUM_TX_QUEUES) 
   {
      //Free up actual list elements in the Tx queue
      hdd_list_destroy( &pAdapter->wmm_tx_queue[i] );
   }

   return status;
}


/**============================================================================
  @brief hdd_disconnect_tx_rx() - Disconnect function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_disconnect_tx_rx( hdd_adapter_t *pAdapter )
{
   return hdd_flush_tx_queues(pAdapter);
}


/**============================================================================
  @brief hdd_IsEAPOLPacket() - Checks the packet is EAPOL or not.

  @param pVosPacket : [in] pointer to vos packet  
  @return         : VOS_TRUE if the packet is EAPOL 
                  : VOS_FALSE otherwise
  ===========================================================================*/

v_BOOL_t hdd_IsEAPOLPacket( vos_pkt_t *pVosPacket )
{
    VOS_STATUS vosStatus  = VOS_STATUS_SUCCESS;
    v_BOOL_t   fEAPOL     = VOS_FALSE; 
    void       *pBuffer   = NULL;

    
    vosStatus = vos_pkt_peek_data( pVosPacket, (v_SIZE_t)HDD_ETHERTYPE_802_1_X_FRAME_OFFSET,
                          &pBuffer, HDD_ETHERTYPE_802_1_X_SIZE );
    if ( VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
       if ( pBuffer && *(unsigned short*)pBuffer ==
                             vos_cpu_to_be16(HDD_ETHERTYPE_802_1_X) )
       {
          fEAPOL = VOS_TRUE;
       }
    }  
    
   return fEAPOL;
}

/**============================================================================
  @brief hdd_IsARP() - Checks the packet is ARP or not.

  @param pVosPacket : [in] pointer to vos packet
  @return         : VOS_TRUE if the packet is ARP
                  : VOS_FALSE otherwise
  ===========================================================================*/

v_BOOL_t hdd_IsARP( vos_pkt_t *pVosPacket )
{
    VOS_STATUS vosStatus  = VOS_STATUS_SUCCESS;
    v_BOOL_t   fIsARP     = VOS_FALSE;
    void       *pBuffer   = NULL;


    vosStatus = vos_pkt_peek_data( pVosPacket,
                           (v_SIZE_t)HDD_ETHERTYPE_802_1_X_FRAME_OFFSET,
                          &pBuffer, HDD_ETHERTYPE_802_1_X_SIZE );
    if ( VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
       if ( pBuffer && *(unsigned short*)pBuffer ==
                                 vos_cpu_to_be16(HDD_ETHERTYPE_ARP) )
       {
          fIsARP = VOS_TRUE;
       }
    }

   return fIsARP;
}

#ifdef FEATURE_WLAN_WAPI // Need to update this function
/**============================================================================
  @brief hdd_IsWAIPacket() - Checks the packet is WAI or not.

  @param pVosPacket : [in] pointer to vos packet
  @return         : VOS_TRUE if the packet is WAI
                  : VOS_FALSE otherwise
  ===========================================================================*/

v_BOOL_t hdd_IsWAIPacket( vos_pkt_t *pVosPacket )
{
    VOS_STATUS vosStatus  = VOS_STATUS_SUCCESS;
    v_BOOL_t   fIsWAI     = VOS_FALSE;
    void       *pBuffer   = NULL;

    // Need to update this function
    vosStatus = vos_pkt_peek_data( pVosPacket, (v_SIZE_t)HDD_ETHERTYPE_802_1_X_FRAME_OFFSET,
                          &pBuffer, HDD_ETHERTYPE_802_1_X_SIZE );

    if (VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
       if ( pBuffer && *(unsigned short*)pBuffer ==
                               vos_cpu_to_be16(HDD_ETHERTYPE_WAI) )
       {
          fIsWAI = VOS_TRUE;
       }
    }

   return fIsWAI;
}
#endif /* FEATURE_WLAN_WAPI */

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
VOS_STATUS hdd_tx_complete_cbk( v_VOID_t *vosContext, 
                                vos_pkt_t *pVosPacket, 
                                VOS_STATUS vosStatusIn )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   hdd_adapter_t *pAdapter = NULL;   
   hdd_context_t *pHddCtx = NULL;
   void* pOsPkt = NULL;
   
   if( ( NULL == vosContext ) || ( NULL == pVosPacket )  )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                       "%s: Null params being passed", __func__);
      return VOS_STATUS_E_FAILURE; 
   }

   //Return the skb to the OS
   status = vos_pkt_get_os_packet( pVosPacket, &pOsPkt, VOS_TRUE );
   if (!VOS_IS_STATUS_SUCCESS( status ))
   {
      //This is bad but still try to free the VOSS resources if we can
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                       "%s: Failure extracting skb from vos pkt", __func__);
      vos_pkt_return_packet( pVosPacket );
      return VOS_STATUS_E_FAILURE;
   }
   
   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context( VOS_MODULE_ID_HDD, vosContext );
   //Get the Adapter context.
   pAdapter = hdd_get_adapter(pHddCtx,WLAN_HDD_INFRA_STATION);
   if (pAdapter == NULL || WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                    "%s: Invalid adapter %p", __func__, pAdapter);
   }
   else
   {
      ++pAdapter->hdd_stats.hddTxRxStats.txCompleted;
   }

   kfree_skb((struct sk_buff *)pOsPkt); 

   //Return the VOS packet resources.
   status = vos_pkt_return_packet( pVosPacket );
   if (!VOS_IS_STATUS_SUCCESS( status ))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                    "%s: Could not return VOS packet to the pool", __func__);
   }

   return status;
}

/**============================================================================
  @brief hdd_ibss_tx_fetch_packet_cbk() - Callback function invoked by TL to
  fetch a packet for transmission.

  @param vosContext   : [in] pointer to VOS context
  @param staId        : [in] Station for which TL is requesting a pkt
  @param ac           : [in] access category requested by TL
  @param pVosPacket   : [out] pointer to VOS packet packet pointer
  @param pPktMetaInfo : [out] pointer to meta info for the pkt

  @return             : VOS_STATUS_E_EMPTY if no packets to transmit
                      : VOS_STATUS_E_FAILURE if any errors encountered
                      : VOS_STATUS_SUCCESS otherwise
 ===========================================================================*/
VOS_STATUS hdd_ibss_tx_fetch_packet_cbk( v_VOID_t *vosContext,
                                    v_U8_t *pStaId,
                                    WLANTL_ACEnumType  ac,
                                    vos_pkt_t **ppVosPacket,
                                    WLANTL_MetaInfoType *pPktMetaInfo )
{
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   hdd_adapter_t *pAdapter = NULL;
   hdd_list_node_t *anchor = NULL;
   skb_list_node_t *pktNode = NULL;
   struct sk_buff *skb = NULL;
   vos_pkt_t *pVosPacket = NULL;
   v_MACADDR_t* pDestMacAddress = NULL;
   v_TIME_t timestamp;
   v_SIZE_t size = 0;
   v_U8_t STAId = WLAN_MAX_STA_COUNT;
   hdd_context_t *pHddCtx = NULL;
   hdd_station_ctx_t *pHddStaCtx = NULL;
   hdd_ibss_peer_info_t *pPeerInfo = NULL;
   v_U8_t proto_type = 0;
   v_U16_t packet_size;

   //Sanity check on inputs
   if ( ( NULL == vosContext ) ||
        ( NULL == pStaId ) ||
        ( NULL == ppVosPacket ) ||
        ( NULL == pPktMetaInfo ) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 "%s: Null Params being passed", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context( VOS_MODULE_ID_HDD, vosContext );
   if ( NULL == pHddCtx )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 "%s: HDD adapter context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   STAId = *pStaId;
   pAdapter = pHddCtx->sta_to_adapter[STAId];
   if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
   {
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

  pHddStaCtx = &pAdapter->sessionCtx.station;
  pPeerInfo = &pHddStaCtx->ibss_peer_info;

  if (FALSE == pPeerInfo->ibssStaInfo[STAId].isUsed )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 "%s: Unregistered STAId %d passed by TL", __func__, STAId);
      return VOS_STATUS_E_FAILURE;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.txFetched;

   *ppVosPacket = NULL;

   //Make sure the AC being asked for is sane
   if( ac > WLANTL_MAX_AC || ac < 0)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 "%s: Invalid AC %d passed by TL", __func__, ac);
      return VOS_STATUS_E_FAILURE;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.txFetchedAC[ac];

   //Get the vos packet before so that we are prepare for VOS low reseurce condition
   //This simplifies the locking and unlocking of Tx queue
   status = vos_pkt_wrap_data_packet( &pVosPacket,
                                      VOS_PKT_TYPE_TX_802_3_DATA,
                                      NULL, //OS Pkt is not being passed
                                      hdd_tx_low_resource_cbk,
                                      pAdapter );

   if ((status == VOS_STATUS_E_ALREADY) || (status == VOS_STATUS_E_RESOURCES))
   {
      //Remember VOS is in a low resource situation
      pAdapter->isVosOutOfResource = VOS_TRUE;
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchLowResources;
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,
                 "%s: VOSS in Low Resource scenario", __func__);
      //TL needs to handle this case. VOS_STATUS_E_EMPTY is returned when the queue is empty.
      return VOS_STATUS_E_FAILURE;
   }

   /* Only fetch this station and this AC. Return VOS_STATUS_E_EMPTY if nothing there.
      Do not get next AC as the other branch does.
   */
   spin_lock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].lock);
   hdd_list_size(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac], &size);

   if (0 == size)
   {
      spin_unlock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].lock);
      vos_pkt_return_packet(pVosPacket);
      return VOS_STATUS_E_EMPTY;
   }

   status = hdd_list_remove_front( &pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac], &anchor );
   spin_unlock_bh(&pPeerInfo->ibssStaInfo[STAId].wmm_tx_queue[ac].lock);

   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                       "%s: AC %d has packets pending", __func__, ac);

   if(VOS_STATUS_SUCCESS == status)
   {
      //If success then we got a valid packet from some AC
      pktNode = list_entry(anchor, skb_list_node_t, anchor);
      skb = pktNode->skb;
   }
   else
   {
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchDequeueError;
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 "%s: Error in de-queuing skb from Tx queue status = %d",
                 __func__, status );
      vos_pkt_return_packet(pVosPacket);
      return VOS_STATUS_E_FAILURE;
   }

   //Attach skb to VOS packet.
   status = vos_pkt_set_os_packet( pVosPacket, skb );
   if (status != VOS_STATUS_SUCCESS)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 "%s: Error attaching skb", __func__);
      vos_pkt_return_packet(pVosPacket);
      ++pAdapter->stats.tx_dropped;
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchDequeueError;
      kfree_skb(skb);
      return VOS_STATUS_E_FAILURE;
   }

   //Return VOS packet to TL;
   *ppVosPacket = pVosPacket;

   //Fill out the meta information needed by TL
   vos_pkt_get_timestamp( pVosPacket, &timestamp );
   pPktMetaInfo->usTimeStamp = (v_U16_t)timestamp;
   if ( 1 < size )
   {
       pPktMetaInfo->bMorePackets = 1; //HDD has more packets to send
   }
   else
   {
       pPktMetaInfo->bMorePackets = 0;
   }

   if(pAdapter->sessionCtx.station.conn_info.uIsAuthenticated == VOS_TRUE)
      pPktMetaInfo->ucIsEapol = 0;
   else
      pPktMetaInfo->ucIsEapol = hdd_IsEAPOLPacket( pVosPacket ) ? 1 : 0;

   if ((NULL != pHddCtx) &&
       (pHddCtx->cfg_ini->gEnableDebugLog))
   {
      proto_type = vos_pkt_get_proto_type(skb,
                                          pHddCtx->cfg_ini->gEnableDebugLog);
      if (VOS_PKT_PROTO_TYPE_EAPOL & proto_type)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "IBSS STA TX EAPOL");
      }
      else if (VOS_PKT_PROTO_TYPE_DHCP & proto_type)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "IBSS STA TX DHCP");
      }
      else if (VOS_PKT_PROTO_TYPE_ARP & proto_type)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "IBSS STA TX ARP");
      }
   }

   vos_pkt_get_packet_length( pVosPacket,&packet_size );
   if ( HDD_ETHERTYPE_ARP_SIZE == packet_size )
       pPktMetaInfo->ucIsArp = hdd_IsARP( pVosPacket ) ? 1 : 0;

   pPktMetaInfo->ucUP = pktNode->userPriority;
   pPktMetaInfo->ucTID = pPktMetaInfo->ucUP;
   pPktMetaInfo->ucType = 0;

   //Extract the destination address from ethernet frame
   pDestMacAddress = (v_MACADDR_t*)skb->data;

   // we need 802.3 to 802.11 frame translation
   // (note that Bcast/Mcast will be translated in SW, unicast in HW)
   pPktMetaInfo->ucDisableFrmXtl = 0;
   pPktMetaInfo->ucBcast = vos_is_macaddr_broadcast( pDestMacAddress ) ? 1 : 0;
   pPktMetaInfo->ucMcast = vos_is_macaddr_group( pDestMacAddress ) ? 1 : 0;

   if ( (pPeerInfo->ibssStaInfo[STAId].txSuspended[ac]) &&
        (size <= ((pAdapter->aTxQueueLimit[ac]*3)/4) ))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                 "%s: TX queue re-enabled", __func__);
      pPeerInfo->ibssStaInfo[STAId].txSuspended[ac] = VOS_FALSE;
      netif_wake_subqueue(pAdapter->dev, skb_get_queue_mapping(skb));
   }

   // We're giving the packet to TL so consider it transmitted from
   // a statistics perspective.  We account for it here instead of
   // when the packet is returned for two reasons.  First, TL will
   // manipulate the skb to the point where the len field is not
   // accurate, leading to inaccurate byte counts if we account for
   // it later.  Second, TL does not provide any feedback as to
   // whether or not the packet was successfully sent over the air,
   // so the packet counts will be the same regardless of where we
   // account for them
   pAdapter->stats.tx_bytes += skb->len;
   ++pAdapter->stats.tx_packets;
   ++pAdapter->hdd_stats.hddTxRxStats.txFetchDequeued;
   ++pAdapter->hdd_stats.hddTxRxStats.txFetchDequeuedAC[ac];
   pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount = 0;

   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
              "%s: Valid VOS PKT returned to TL", __func__);

   return status;
}

/**============================================================================
  @brief hdd_tx_fetch_packet_cbk() - Callback function invoked by TL to 
  fetch a packet for transmission.

  @param vosContext   : [in] pointer to VOS context  
  @param staId        : [in] Station for which TL is requesting a pkt
  @param ac           : [in] access category requested by TL
  @param pVosPacket   : [out] pointer to VOS packet packet pointer
  @param pPktMetaInfo : [out] pointer to meta info for the pkt 
  
  @return             : VOS_STATUS_E_EMPTY if no packets to transmit
                      : VOS_STATUS_E_FAILURE if any errors encountered 
                      : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_tx_fetch_packet_cbk( v_VOID_t *vosContext,
                                    v_U8_t *pStaId,
                                    WLANTL_ACEnumType  ac,
                                    vos_pkt_t **ppVosPacket,
                                    WLANTL_MetaInfoType *pPktMetaInfo )
{
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   hdd_adapter_t *pAdapter = NULL;
   hdd_context_t *pHddCtx = NULL;
   hdd_list_node_t *anchor = NULL;
   skb_list_node_t *pktNode = NULL;
   struct sk_buff *skb = NULL;
   vos_pkt_t *pVosPacket = NULL;
   v_MACADDR_t* pDestMacAddress = NULL;
   v_TIME_t timestamp;
   WLANTL_ACEnumType newAc;
   v_SIZE_t size = 0;
   v_U16_t packet_size;
   tANI_U8   acAdmitted, i;
   v_U8_t proto_type = 0;
   WLANTL_ACEnumType actualAC;

   //Sanity check on inputs
   if ( ( NULL == vosContext ) || 
        ( NULL == pStaId ) || 
        ( NULL == ppVosPacket ) ||
        ( NULL == pPktMetaInfo ) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                          "%s: Null Params being passed", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   //Get the HDD context.
   pHddCtx = (hdd_context_t *)vos_get_context( VOS_MODULE_ID_HDD, vosContext );
   if(pHddCtx == NULL)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                        "%s: HDD adapter context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }
   pAdapter = pHddCtx->sta_to_adapter[*pStaId];
   if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("invalid adapter:%p for staId:%u"), pAdapter, *pStaId);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.txFetched;

   *ppVosPacket = NULL;

   //Make sure the AC being asked for is sane
   if (ac > WLANTL_AC_HIGH_PRIO || ac < 0)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                            "%s: Invalid QId %d passed by TL", __func__, ac);
      return VOS_STATUS_E_FAILURE;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.txFetchedAC[ac];

#ifdef HDD_WMM_DEBUG
   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
                              "%s: AC %d passed by TL", __func__, ac);
#endif // HDD_WMM_DEBUG

   // do we have any packets pending in this AC?
   hdd_list_size( &pAdapter->wmm_tx_queue[ac], &size ); 
   if( size >  0 )
   {
       // yes, so process it
#ifdef HDD_WMM_DEBUG
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
                       "%s: AC %d has packets pending", __func__, ac);
#endif // HDD_WMM_DEBUG
   }
   else
   {
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchEmpty;
#ifdef HDD_WMM_DEBUG
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
                   "%s: no packets pending", __func__);
#endif // HDD_WMM_DEBUG
      return VOS_STATUS_E_FAILURE;
   }

   // Note here that we are not checking "wmmAcAccessAllowed" for packets
   // in new queue since there is no one AC associated to the new queue.
   // Since there will be either eapol or dhcp pkts in new queue overlooking
   // this should be okay from implicit QoS perspective.
   if (ac != WLANTL_AC_HIGH_PRIO)
   {
      // We find an AC with packets
      // or we determine we have no more packets to send
      // HDD is not allowed to change AC.

      // has this AC been admitted? or
      // To allow EAPOL packets when not authenticated
      if (unlikely((0==pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessAllowed) &&
                   (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.uIsAuthenticated))
      {
         ++pAdapter->hdd_stats.hddTxRxStats.txFetchEmpty;
#ifdef HDD_WMM_DEBUG
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
                    "%s: no packets pending", __func__);
#endif // HDD_WMM_DEBUG
         return VOS_STATUS_E_FAILURE;
      }
   }

   //Get the vos packet. I don't want to dequeue and enqueue again if we are out of VOS resources 
   //This simplifies the locking and unlocking of Tx queue
   status = vos_pkt_wrap_data_packet( &pVosPacket, 
                                      VOS_PKT_TYPE_TX_802_3_DATA, 
                                      NULL, //OS Pkt is not being passed
                                      hdd_tx_low_resource_cbk, 
                                      pAdapter );

   if (status == VOS_STATUS_E_ALREADY || status == VOS_STATUS_E_RESOURCES)
   {
      //Remember VOS is in a low resource situation
      pAdapter->isVosOutOfResource = VOS_TRUE;
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchLowResources;
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,"%s: VOSS in Low Resource scenario", __func__);
      //TL will now think we have no more packets in this AC
      return VOS_STATUS_E_FAILURE;
   }

   //Remove the packet from the queue
   spin_lock_bh(&pAdapter->wmm_tx_queue[ac].lock);
   status = hdd_list_remove_front( &pAdapter->wmm_tx_queue[ac], &anchor );
   spin_unlock_bh(&pAdapter->wmm_tx_queue[ac].lock);

   if(VOS_STATUS_SUCCESS == status)
   {
      //If success then we got a valid packet from some AC
      pktNode = list_entry(anchor, skb_list_node_t, anchor);
      skb = pktNode->skb;
      actualAC = hddWmmUpToAcMap[pktNode->userPriority];
      if (actualAC >= WLANTL_MAX_AC)
      {
         /* To fix klocwork */
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,
         "%s: Invalid AC for packet:%d", __func__, actualAC);
         actualAC = WLANTL_AC_BE;
      }
   }
   else
   {
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchDequeueError;
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN, "%s: Error in de-queuing "
         "skb from Tx queue status = %d", __func__, status );
      vos_pkt_return_packet(pVosPacket);
      return VOS_STATUS_E_FAILURE;
   }
   //Attach skb to VOS packet.
   status = vos_pkt_set_os_packet( pVosPacket, skb );
   if (status != VOS_STATUS_SUCCESS)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,"%s: Error attaching skb", __func__);
      vos_pkt_return_packet(pVosPacket);
      ++pAdapter->stats.tx_dropped;
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchDequeueError;
      kfree_skb(skb);
      return VOS_STATUS_E_FAILURE;
   }

   //Just being paranoid. To be removed later
   if(pVosPacket == NULL)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,"%s: VOS packet returned by VOSS is NULL", __func__);
      ++pAdapter->stats.tx_dropped;
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchDequeueError;
      kfree_skb(skb);
      return VOS_STATUS_E_FAILURE;
   }

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
   if (vos_is_macaddr_multicast((v_MACADDR_t*)skb->data))
   {
        pAdapter->hdd_stats.hddTxRxStats.txMcast[actualAC]++;
   }

#endif

#ifdef FEATURE_WLAN_TDLS
    if (eTDLS_SUPPORT_ENABLED == pHddCtx->tdls_mode)
    {
        hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
        u8 mac[6];

        wlan_hdd_tdls_extract_da(skb, mac);

        if (vos_is_macaddr_group((v_MACADDR_t *)mac)) {
            VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_MED,
                      "broadcast packet, not adding to peer list");
        } else if (memcmp(pHddStaCtx->conn_info.bssId,
                            mac, 6) != 0) {
            VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_MED,
                      "extract mac: " MAC_ADDRESS_STR,
                      MAC_ADDR_ARRAY(mac) );

            wlan_hdd_tdls_increment_pkt_count(pAdapter, mac, 1);
        } else {
            VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_MED,
                       "packet da is bssid, not adding to peer list");
        }
    }
#endif

   //Return VOS packet to TL;
   *ppVosPacket = pVosPacket;

   //Fill out the meta information needed by TL
   //FIXME This timestamp is really the time stamp of wrap_data_packet
   vos_pkt_get_timestamp( pVosPacket, &timestamp );
   pPktMetaInfo->usTimeStamp = (v_U16_t)timestamp;
   
   if(pAdapter->sessionCtx.station.conn_info.uIsAuthenticated == VOS_TRUE)
      pPktMetaInfo->ucIsEapol = 0;       
   else
      pPktMetaInfo->ucIsEapol = hdd_IsEAPOLPacket( pVosPacket ) ? 1 : 0;

   if (pPktMetaInfo->ucIsEapol)
       wlan_hdd_log_eapol(skb, WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED);
   if ((NULL != pHddCtx) &&
       (pHddCtx->cfg_ini->gEnableDebugLog))
   {
      proto_type = vos_pkt_get_proto_type(skb,
                                          pHddCtx->cfg_ini->gEnableDebugLog);
      if (VOS_PKT_PROTO_TYPE_EAPOL & proto_type)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "STA TX EAPOL");
      }
      else if (VOS_PKT_PROTO_TYPE_DHCP & proto_type)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "STA TX DHCP");
      }
      else if (VOS_PKT_PROTO_TYPE_ARP & proto_type)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "STA TX ARP");
      }
   }

   vos_pkt_get_packet_length( pVosPacket,&packet_size );
   if( HDD_ETHERTYPE_ARP_SIZE == packet_size )
      pPktMetaInfo->ucIsArp = hdd_IsARP( pVosPacket ) ? 1 : 0;

#ifdef FEATURE_WLAN_WAPI
   // Override usIsEapol value when its zero for WAPI case
   pPktMetaInfo->ucIsWai = hdd_IsWAIPacket( pVosPacket ) ? 1 : 0;
#endif /* FEATURE_WLAN_WAPI */

   /* 1. Check if ACM is set for this AC
    * 2. If set, check if this AC had already admitted
    * 3. If not already admitted, downgrade the UP to next best UP
    * 4. Allow only when medium time is non zero when Addts accepted
    *    else downgrade traffic. we opted downgrading over Delts when
    *    medium time is zero because while doing downgradig driver is not
    *    clearing the wmm context so consider in subsequent roaming
    *    if AP (new or same AP) accept the Addts with valid medium time
    *    no application support is required where if we have opted
    *    delts Applications have to again do Addts or STA will never
    *    go for Addts.
    */
   pPktMetaInfo->ac = actualAC;
   if(!pAdapter->hddWmmStatus.wmmAcStatus[actualAC].wmmAcAccessRequired ||
      (pAdapter->hddWmmStatus.wmmAcStatus[actualAC].wmmAcTspecValid &&
       pAdapter->hddWmmStatus.wmmAcStatus[actualAC].wmmAcTspecInfo.medium_time))
   {
      pPktMetaInfo->ucUP = pktNode->userPriority;
      pPktMetaInfo->ucTID = pPktMetaInfo->ucUP;
   }
   else
   {
     //Downgrade the UP
      acAdmitted = pAdapter->hddWmmStatus.wmmAcStatus[actualAC].wmmAcTspecValid;
      newAc = WLANTL_AC_BK;
      for (i=actualAC-1; i>0; i--)
      {
         if (pAdapter->hddWmmStatus.wmmAcStatus[i].wmmAcAccessRequired == 0)
         {
             newAc = i;
             break;
         }
      }
      pPktMetaInfo->ucUP = hddWmmAcToHighestUp[newAc];
      pPktMetaInfo->ucTID = pPktMetaInfo->ucUP;
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_LOW,
               "Downgrading UP %d to UP %d ",
                pktNode->userPriority, pPktMetaInfo->ucUP);
   }

   if (pHddCtx->cfg_ini->gEnableRoamDelayStats)
   {
       vos_record_roam_event(e_TL_FIRST_XMIT_TIME, NULL, 0);
   }

   pPktMetaInfo->ucType = 0;          //FIXME Don't know what this is
   pPktMetaInfo->ucDisableFrmXtl = 0; //802.3 frame so we need to xlate
   if ( 1 < size )
   {
       pPktMetaInfo->bMorePackets = 1; //HDD has more packets to send
   }
   else
   {
       pPktMetaInfo->bMorePackets = 0;
   }

   //Extract the destination address from ethernet frame
   pDestMacAddress = (v_MACADDR_t*)skb->data;
   pPktMetaInfo->ucBcast = vos_is_macaddr_broadcast( pDestMacAddress ) ? 1 : 0;
   pPktMetaInfo->ucMcast = vos_is_macaddr_group( pDestMacAddress ) ? 1 : 0;

   

   // if we are in a backpressure situation see if we can turn the hose back on
   if ( (pAdapter->isTxSuspended[ac]) &&
        (size <= HDD_TX_QUEUE_LOW_WATER_MARK) )
   {
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchDePressured;
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchDePressuredAC[ac];
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                 "%s: TX queue[%d] re-enabled", __func__, ac);
      pAdapter->isTxSuspended[ac] = VOS_FALSE;      
      netif_tx_wake_queue(netdev_get_tx_queue(pAdapter->dev, 
                                        skb_get_queue_mapping(skb) ));
      MTRACE(vos_trace(VOS_MODULE_ID_HDD, TRACE_CODE_HDD_WAKE_NETDEV,
                       pAdapter->sessionId, ac));
   }


   // We're giving the packet to TL so consider it transmitted from
   // a statistics perspective.  We account for it here instead of
   // when the packet is returned for two reasons.  First, TL will
   // manipulate the skb to the point where the len field is not
   // accurate, leading to inaccurate byte counts if we account for
   // it later.  Second, TL does not provide any feedback as to
   // whether or not the packet was successfully sent over the air,
   // so the packet counts will be the same regardless of where we
   // account for them
   pAdapter->stats.tx_bytes += skb->len;
   ++pAdapter->stats.tx_packets;
   ++pAdapter->hdd_stats.hddTxRxStats.txFetchDequeued;
   ++pAdapter->hdd_stats.hddTxRxStats.txFetchDequeuedAC[ac];
   pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount = 0;

   if((pHddCtx->cfg_ini->thermalMitigationEnable) &&
      (WLAN_HDD_INFRA_STATION == pAdapter->device_mode))
   {
      if(mutex_lock_interruptible(&pHddCtx->tmInfo.tmOperationLock))
      {
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                    "%s: Tm Lock fail", __func__);
         return VOS_STATUS_E_FAILURE;
      }
      if(WLAN_HDD_TM_LEVEL_1 < pHddCtx->tmInfo.currentTmLevel)
      {
         if(0 == pHddCtx->tmInfo.txFrameCount)
         {
            /* Just recovered from sleep timeout */
            pHddCtx->tmInfo.lastOpenTs = timestamp;
         }

         if((VOS_FALSE == pHddCtx->tmInfo.qBlocked) &&
            ((timestamp - pHddCtx->tmInfo.lastOpenTs) > (pHddCtx->tmInfo.tmAction.txOperationDuration / 10)) &&
            (pHddCtx->tmInfo.txFrameCount >= pHddCtx->tmInfo.tmAction.txBlockFrameCountThreshold))
         {
            /* During TX open duration, TX frame count is larger than threshold
             * Block TX during Sleep time */
            hddLog(VOS_TRACE_LEVEL_INFO, FL("Disabling queues"));
            netif_tx_stop_all_queues(pAdapter->dev);
            pHddCtx->tmInfo.qBlocked = VOS_TRUE;
            pHddCtx->tmInfo.lastblockTs = timestamp;
            if(VOS_TIMER_STATE_STOPPED == vos_timer_getCurrentState(&pHddCtx->tmInfo.txSleepTimer))
            {
               vos_timer_start(&pHddCtx->tmInfo.txSleepTimer, pHddCtx->tmInfo.tmAction.txSleepDuration);
            }
         }
         else if(((timestamp - pHddCtx->tmInfo.lastOpenTs) > (pHddCtx->tmInfo.tmAction.txOperationDuration / 10)) &&
                 (pHddCtx->tmInfo.txFrameCount < pHddCtx->tmInfo.tmAction.txBlockFrameCountThreshold))
         {
            /* During TX open duration, TX frame count is less than threshold
             * Reset count and timestamp to prepare next cycle */
            pHddCtx->tmInfo.lastOpenTs = timestamp;
            pHddCtx->tmInfo.txFrameCount = 0;
         }
         else
         {
            /* Do Nothing */
         }
         pHddCtx->tmInfo.txFrameCount++;
      }
      mutex_unlock(&pHddCtx->tmInfo.tmOperationLock);
   }


#ifdef HDD_WMM_DEBUG
   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,"%s: Valid VOS PKT returned to TL", __func__);
#endif // HDD_WMM_DEBUG

   return status;
}


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
VOS_STATUS hdd_tx_low_resource_cbk( vos_pkt_t *pVosPacket, 
                                    v_VOID_t *userData )
{
   VOS_STATUS status;
   v_SINT_t i = 0;
   v_SIZE_t size = 0;
   hdd_adapter_t* pAdapter = (hdd_adapter_t *)userData;
   
   if (NULL == pAdapter || WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 FL("Invalid adapter %p"), pAdapter);
      return VOS_STATUS_E_FAILURE;
   }

   //Return the packet to VOS. We just needed to know that VOS is out of low resource
   //situation. Here we will only signal TL that there is a pending data for a STA. 
   //VOS packet will be requested (if needed) when TL comes back to fetch data.
   vos_pkt_return_packet( pVosPacket );

   pAdapter->isVosOutOfResource = VOS_FALSE;

   //Indicate to TL that there is pending data if a queue is non empty
   for( i=NUM_TX_QUEUES-1; i>=0; --i )
   {
      size = 0;
      hdd_list_size( &pAdapter->wmm_tx_queue[i], &size );
      if ( size > 0 )
      {
         status = WLANTL_STAPktPending( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext, 
                                        (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId [0], 
                                        (WLANTL_ACEnumType)i );
         if( !VOS_IS_STATUS_SUCCESS( status ) )
         {
            VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                           "%s: Failure in indicating pkt to TL for ac=%d", __func__, i);
         }
      }
   }

   return VOS_STATUS_SUCCESS;
}

static void hdd_mon_add_rx_radiotap_hdr (struct sk_buff *skb,
             int rtap_len,  v_PVOID_t  pRxPacket, hdd_mon_ctx_t *pMonCtx)
{
    u8 rtap_temp[20] = {0};
    struct ieee80211_radiotap_header *rthdr;
    unsigned char *pos;
    u16 rx_flags = 0;
    u16 rateIdx;
    s8        currentRSSI, currentRSSI0, currentRSSI1;

    rateIdx = WDA_GET_RX_MAC_RATE_IDX(pRxPacket);
    if( rateIdx >= 210 && rateIdx <= 217)
       rateIdx-=202;
    if( rateIdx >= 218 && rateIdx <= 225 )
       rateIdx-=210;

    if(rateIdx > (sizeof(gRatefromIdx)/ sizeof(int))) {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                  "%s: invalid rateIdx %d make it 0", __func__, rateIdx);
       rateIdx = 0;
    }
    currentRSSI0 = WDA_GETRSSI0(pRxPacket) - 100;
    currentRSSI1 = WDA_GETRSSI1(pRxPacket) - 100;
    currentRSSI  = (currentRSSI0 > currentRSSI1) ? currentRSSI0 : currentRSSI1;

    rthdr = (struct ieee80211_radiotap_header *)(&rtap_temp[0]);

    /* radiotap header, set always present flags */
    rthdr->it_present = cpu_to_le32((1 << IEEE80211_RADIOTAP_FLAGS)   |
                                    (1 << IEEE80211_RADIOTAP_CHANNEL) |
                                    (1 << IEEE80211_RADIOTAP_RX_FLAGS));
    rthdr->it_len = cpu_to_le16(rtap_len);

    pos = (unsigned char *) (rthdr + 1);

    /* IEEE80211_RADIOTAP_FLAGS */
    *pos = 0;
    pos++;

    /* IEEE80211_RADIOTAP_RATE */
    rthdr->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RATE);
    *pos = gRatefromIdx[rateIdx]/5;

    pos++;

    /* IEEE80211_RADIOTAP_CHANNEL */
    put_unaligned_le16(pMonCtx->ChannelNo, pos);
    pos += 4;

    /* IEEE80211_RADIOTAP_DBM_ANTSIGNAL */
    *pos = currentRSSI;
    rthdr->it_present |=cpu_to_le32(1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL);
    pos++;

    if ((pos - (u8 *)rthdr) & 1)
        pos++;
    put_unaligned_le16(rx_flags, pos);
    pos += 2;

    memcpy(skb_push(skb, rtap_len), &rtap_temp[0], rtap_len);
}


VOS_STATUS  hdd_rx_packet_monitor_cbk(v_VOID_t *vosContext,vos_pkt_t *pVosPacket, int conversion)
{
  struct sk_buff *skb = NULL;
  VOS_STATUS status = VOS_STATUS_E_FAILURE;
  hdd_adapter_t *pAdapter = NULL;
  hdd_context_t *pHddCtx = NULL;
  hdd_mon_ctx_t *pMonCtx = NULL;
  v_PVOID_t pvBDHeader = NULL;
  int rxstat;
  int needed_headroom = 0;


  //Sanity check on inputs
   if ( ( NULL == vosContext ) ||
        ( NULL == pVosPacket ) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                         "%s: Null params being passed", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   pHddCtx = (hdd_context_t *)vos_get_context( VOS_MODULE_ID_HDD, vosContext );

   if (NULL == pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 FL("Failed to get pHddCtx from vosContext"));
      vos_pkt_return_packet( pVosPacket );
      return VOS_STATUS_E_FAILURE;
   }

   pAdapter = hdd_get_adapter(pHddCtx,WLAN_HDD_MONITOR);
   if ((NULL == pAdapter)  || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic) )
   {
      VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 FL("Invalid adapter %p for MONITOR MODE"), pAdapter);
      vos_pkt_return_packet( pVosPacket );
      return VOS_STATUS_E_FAILURE;
   }

    status =  WDA_DS_PeekRxPacketInfo( pVosPacket, (v_PVOID_t)&pvBDHeader, 1/*Swap BD*/ );
    if ( NULL == pvBDHeader )
    {
      VOS_TRACE( VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                 "Cannot extract BD header");
      /* Drop packet */
      vos_pkt_return_packet(pVosPacket);
      return VOS_STATUS_E_FAILURE;
    }

   ++pAdapter->hdd_stats.hddTxRxStats.rxChains;
   status = vos_pkt_get_os_packet( pVosPacket, (v_VOID_t **)&skb, VOS_TRUE );
   if(!VOS_IS_STATUS_SUCCESS( status ))
    {
        ++pAdapter->hdd_stats.hddTxRxStats.rxDropped;
        VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                               "%s: Failure extracting skb from vos pkt", __func__);
        vos_pkt_return_packet( pVosPacket );
        return VOS_STATUS_E_FAILURE;
    }

    if(!conversion)
    {
         pMonCtx = WLAN_HDD_GET_MONITOR_CTX_PTR(pAdapter);
         needed_headroom = sizeof(struct ieee80211_radiotap_header) + 10;
         hdd_mon_add_rx_radiotap_hdr( skb, needed_headroom, pvBDHeader, pMonCtx );
    }

    skb_reset_mac_header( skb );
    skb->dev = pAdapter->dev;
    skb->pkt_type = PACKET_OTHERHOST;
    skb->protocol = htons(ETH_P_802_2);
    skb->ip_summed = CHECKSUM_NONE;
    ++pAdapter->hdd_stats.hddTxRxStats.rxPackets;
    ++pAdapter->stats.rx_packets;
    pAdapter->stats.rx_bytes += skb->len;

    rxstat = netif_rx_ni(skb);
    if (NET_RX_SUCCESS == rxstat)
    {
       ++pAdapter->hdd_stats.hddTxRxStats.rxDelivered;
       ++pAdapter->hdd_stats.hddTxRxStats.pkt_rx_count;
    }
    else
    {
       ++pAdapter->hdd_stats.hddTxRxStats.rxRefused;
    }

   status = vos_pkt_return_packet( pVosPacket );
   if(!VOS_IS_STATUS_SUCCESS( status ))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: Failure returning vos pkt", __func__);
   }
   pAdapter->dev->last_rx = jiffies;

return status;
}

/**============================================================================
  @brief hdd_rx_packet_cbk() - Receive callback registered with TL.
  TL will call this to notify the HDD when one or more packets were
  received for a registered STA.

  @param vosContext      : [in] pointer to VOS context  
  @param pVosPacketChain : [in] pointer to VOS packet chain
  @param staId           : [in] Station Id
  @param pRxMetaInfo     : [in] pointer to meta info for the received pkt(s) 

  @return                : VOS_STATUS_E_FAILURE if any errors encountered, 
                         : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_rx_packet_cbk( v_VOID_t *vosContext, 
                              vos_pkt_t *pVosPacketChain,
                              v_U8_t staId,
                              WLANTL_RxMetaInfoType* pRxMetaInfo )
{
   hdd_adapter_t *pAdapter = NULL;
   hdd_context_t *pHddCtx = NULL;
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   int rxstat;
   struct sk_buff *skb = NULL;
   vos_pkt_t* pVosPacket;
   vos_pkt_t* pNextVosPacket;
   v_U8_t proto_type;

   //Sanity check on inputs
   if ( ( NULL == vosContext ) || 
        ( NULL == pVosPacketChain ) ||
        ( NULL == pRxMetaInfo ) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                         "%s: Null params being passed", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   pHddCtx = (hdd_context_t *)vos_get_context( VOS_MODULE_ID_HDD, vosContext );
   if ( NULL == pHddCtx )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                           "%s: HDD adapter context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   pAdapter = pHddCtx->sta_to_adapter[staId];
   if ((NULL == pAdapter)  || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic) )
   {
      VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                 FL("Invalid adapter %p for staId %u"), pAdapter, staId);
      return VOS_STATUS_E_FAILURE;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.rxChains;

   // walk the chain until all are processed
   pVosPacket = pVosPacketChain;
   do
   {
      // get the pointer to the next packet in the chain
      // (but don't unlink the packet since we free the entire chain later)
      status = vos_pkt_walk_packet_chain( pVosPacket, &pNextVosPacket, VOS_FALSE);

      // both "success" and "empty" are acceptable results
      if (!((status == VOS_STATUS_SUCCESS) || (status == VOS_STATUS_E_EMPTY)))
      {
         ++pAdapter->hdd_stats.hddTxRxStats.rxDropped;
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                         "%s: Failure walking packet chain", __func__);
         return VOS_STATUS_E_FAILURE;
      }

      // Extract the OS packet (skb).
      status = vos_pkt_get_os_packet( pVosPacket, (v_VOID_t **)&skb, VOS_FALSE );
      if(!VOS_IS_STATUS_SUCCESS( status ))
      {
         ++pAdapter->hdd_stats.hddTxRxStats.rxDropped;
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                                "%s: Failure extracting skb from vos pkt", __func__);
         return VOS_STATUS_E_FAILURE;
      }

      if (TRUE == hdd_IsEAPOLPacket( pVosPacket ))
          wlan_hdd_log_eapol(skb, WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED);

      pVosPacket->pSkb = NULL;

      if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
           "Magic cookie(%x) for adapter sanity verification is invalid", pAdapter->magic);
         return eHAL_STATUS_FAILURE;
      }

#ifdef FEATURE_WLAN_TDLS
    if ((eTDLS_SUPPORT_ENABLED == pHddCtx->tdls_mode) &&
         0 != pHddCtx->connected_peer_count)
    {
        hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
        u8 mac[6];

        memcpy(mac, skb->data+6, 6);

        if (vos_is_macaddr_group((v_MACADDR_t *)mac)) {
            VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_MED,
                      "rx broadcast packet, not adding to peer list");
        } else if ((memcmp(pHddStaCtx->conn_info.bssId,
                            mac, 6) != 0) && (pRxMetaInfo->isStaTdls)) {
            wlan_hdd_tdls_update_rx_pkt_cnt_n_rssi(pAdapter, mac,
                    pRxMetaInfo->rssiAvg);
        } else {
            VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_MED,
                       "rx packet sa is bssid, not adding to peer list");
        }
    }
#endif
      if (pHddCtx->cfg_ini->gEnableDebugLog)
      {
         proto_type = vos_pkt_get_proto_type(skb,
                                             pHddCtx->cfg_ini->gEnableDebugLog);
         if (VOS_PKT_PROTO_TYPE_EAPOL & proto_type)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "STA RX EAPOL");
         }
         else if (VOS_PKT_PROTO_TYPE_DHCP & proto_type)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "STA RX DHCP");
         }
         else if (VOS_PKT_PROTO_TYPE_ARP & proto_type)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "STA RX ARP");
         }
      }

      if (pHddCtx->cfg_ini->gEnableRoamDelayStats)
      {
          vos_record_roam_event(e_HDD_RX_PKT_CBK_TIME, (void *)skb, 0);
      }
      if (( NULL != pHddCtx ) &&
         (pHddCtx->cfg_ini->gEnableDebugLog & VOS_PKT_PROTO_TYPE_DHCP))
      {
          hdd_dump_dhcp_pkt(skb, RX_PATH);
      }

      skb->dev = pAdapter->dev;
      skb->protocol = eth_type_trans(skb, skb->dev);
      skb->ip_summed = CHECKSUM_NONE;
      ++pAdapter->hdd_stats.hddTxRxStats.rxPackets;
      ++pAdapter->stats.rx_packets;
      pAdapter->stats.rx_bytes += skb->len;
#ifdef WLAN_FEATURE_HOLD_RX_WAKELOCK
       vos_wake_lock_timeout_release(&pHddCtx->rx_wake_lock,
                          HDD_WAKE_LOCK_DURATION,
                          WIFI_POWER_EVENT_WAKELOCK_HOLD_RX);
#endif
      rxstat = netif_rx_ni(skb);
      if (NET_RX_SUCCESS == rxstat)
      {
         ++pAdapter->hdd_stats.hddTxRxStats.rxDelivered;
         ++pAdapter->hdd_stats.hddTxRxStats.pkt_rx_count;
      }
      else
      {
         ++pAdapter->hdd_stats.hddTxRxStats.rxRefused;
      }
      // now process the next packet in the chain
      pVosPacket = pNextVosPacket;

   } while (pVosPacket);

   //Return the entire VOS packet chain to the resource pool
   status = vos_pkt_return_packet( pVosPacketChain );
   if(!VOS_IS_STATUS_SUCCESS( status ))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: Failure returning vos pkt", __func__);
   }
   
   pAdapter->dev->last_rx = jiffies;

   return status;   
}
/**============================================================================
  @brief hdd_tx_rx_pkt_cnt_stat_timer_handler() -
               Enable/Disable split scan based on TX and RX traffic.
  @param HddContext      : [in] pointer to Hdd context
  @return                : None
  ===========================================================================*/
void hdd_tx_rx_pkt_cnt_stat_timer_handler( void *phddctx)
{
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    hdd_adapter_t *pAdapter = NULL;
    hdd_station_ctx_t *pHddStaCtx = NULL;
    hdd_context_t *pHddCtx = (hdd_context_t *)phddctx;
    hdd_config_t  *cfg_param = pHddCtx->cfg_ini;
    VOS_STATUS status;
    v_U8_t staId = 0;
    v_U8_t fconnected = 0;

    ENTER();
    if (0 != (wlan_hdd_validate_context(pHddCtx)))
    {
        return;
    }
    if (!cfg_param->dynSplitscan)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                "%s: Error : Dynamic split scan is not Enabled : %d",
                __func__, pHddCtx->cfg_ini->dynSplitscan);
        return;
    }

    status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
        pAdapter = pAdapterNode->pAdapter;

        if ((NULL != pAdapter) && (WLAN_HDD_ADAPTER_MAGIC == pAdapter->magic))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                    "%s: Adapter with device mode %d exists",
                    __func__, pAdapter->device_mode);

            if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
                    (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))
            {
                pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
                if ((eConnectionState_Associated ==
                                 pHddStaCtx->conn_info.connState) &&
                    (VOS_TRUE == pHddStaCtx->conn_info.uIsAuthenticated))
                {
                    fconnected = TRUE;
                }
            }
            else if ((WLAN_HDD_SOFTAP == pAdapter->device_mode) ||
                     (WLAN_HDD_P2P_GO == pAdapter->device_mode))
            {
                v_CONTEXT_t pVosContext = ( WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
                ptSapContext pSapCtx = VOS_GET_SAP_CB(pVosContext);
                if(pSapCtx == NULL){
                    VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                             FL("psapCtx is NULL"));
                    return;
                }
                for (staId = 0; staId < WLAN_MAX_STA_COUNT; staId++)
                {
                    if ((pSapCtx->aStaInfo[staId].isUsed) &&
                        (WLANTL_STA_AUTHENTICATED ==
                                          pSapCtx->aStaInfo[staId].tlSTAState))
                    {
                        fconnected = TRUE;
                    }
                }
            }
            if ( fconnected )
            {
                VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                        "%s: One of the interface is connected check for scan",
                        __func__);
                VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                       "%s: pkt_tx_count: %d, pkt_rx_count: %d "
                       "miracast = %d", __func__,
                        pAdapter->hdd_stats.hddTxRxStats.pkt_tx_count,
                        pAdapter->hdd_stats.hddTxRxStats.pkt_rx_count,
                        pHddCtx->drvr_miracast);

                vos_timer_start(&pHddCtx->tx_rx_trafficTmr,
                                 cfg_param->trafficMntrTmrForSplitScan);
                //Check for the previous statistics count
                if ((pAdapter->hdd_stats.hddTxRxStats.pkt_tx_count >
                                       cfg_param->txRxThresholdForSplitScan) ||
                    (pAdapter->hdd_stats.hddTxRxStats.pkt_rx_count >
                                       cfg_param->txRxThresholdForSplitScan) ||
                    pHddCtx->drvr_miracast ||
                    (WLAN_HDD_P2P_GO == pAdapter->device_mode))
                {
                    pAdapter->hdd_stats.hddTxRxStats.pkt_tx_count = 0;
                    pAdapter->hdd_stats.hddTxRxStats.pkt_rx_count = 0;

                    if (!pHddCtx->issplitscan_enabled)
                    {
                        pHddCtx->issplitscan_enabled = TRUE;
                        sme_enable_disable_split_scan(
                                            WLAN_HDD_GET_HAL_CTX(pAdapter),
                                            cfg_param->nNumStaChanCombinedConc,
                                            cfg_param->nNumP2PChanCombinedConc);
                    }
                    return;
                }
                else
                {
                    pAdapter->hdd_stats.hddTxRxStats.pkt_tx_count = 0;
                    pAdapter->hdd_stats.hddTxRxStats.pkt_rx_count = 0;
                }
                fconnected = FALSE;
            }
        }
        status = hdd_get_next_adapter( pHddCtx, pAdapterNode, &pNext);
        pAdapterNode = pNext;
    }

    /* If TDLSScanCoexistence is enabled, then the TDLS module shall take care
     * of disabling the split scan and thus do not disable the same when the
     * low TXRX condition is met.
     */
    if ((pHddCtx->isTdlsScanCoexistence == FALSE) && (pHddCtx->issplitscan_enabled))
    {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                        "%s: Disable split scan", __func__);
       pHddCtx->issplitscan_enabled = FALSE;
       sme_enable_disable_split_scan(
                                  pHddCtx->hHal,
                                  SME_DISABLE_SPLIT_SCAN,
                                  SME_DISABLE_SPLIT_SCAN);
    }
    EXIT();
    return;
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/*
 * wlan_hdd_get_eapol_params() - Function to extract EAPOL params
 * @skb:                skb data
 * @eapol_params:       Pointer to hold the parsed EAPOL params
 * @event_type:         Event type to indicate Tx/Rx
 *
 * This function parses the input skb data to get the EAPOL params,if the
 * packet is EAPOL and store it in the pointer passed as input
 *
 */
void  wlan_hdd_get_eapol_params(struct sk_buff *skb,
                                    struct vos_event_wlan_eapol *eapol_params,
                                    uint8_t event_type)
{
     uint8_t packet_type=0;

     packet_type = (uint8_t)(*(uint8_t *)
                   (skb->data + HDD_EAPOL_PACKET_TYPE_OFFSET));

    /* EAPOL msg type  i.e. whether EAPOL-Start or
     * EAPOL-Key etc. messages Present at 15 offset.
     */
     eapol_params->eapol_packet_type = packet_type;

    /* This tells if its a M1/M2/M3/M4 packet.
     * Present at 19th offset in EAPOL frame.
     */
     eapol_params->eapol_key_info = (uint16_t)(*(uint16_t *)
                                    (skb->data + HDD_EAPOL_KEY_INFO_OFFSET));
    /* This tells if EAPOL packet is in RX or TX
     * direction.
     */
     eapol_params->event_sub_type = event_type;

    /* This tells the rate at which EAPOL packet
     * is send or received.
     */
     //TODO fill data rate for rx packet.
     eapol_params->eapol_rate = 0;/* As of now, zero */

     vos_mem_copy(eapol_params->dest_addr,
                 (skb->data + HDD_EAPOL_DEST_MAC_OFFSET),
                  sizeof(eapol_params->dest_addr));
     vos_mem_copy(eapol_params->src_addr,
                 (skb->data + HDD_EAPOL_SRC_MAC_OFFSET),
                  sizeof(eapol_params->src_addr));
     return;
}
/*
 * wlan_hdd_event_eapol_log() - Function to log EAPOL events
 * @eapol_params:    Structure containing EAPOL params
 *
 * This function logs the parsed EAPOL params
 *
 * Return: None
 *
 */

static void wlan_hdd_event_eapol_log(struct vos_event_wlan_eapol eapol_params)
{
    WLAN_VOS_DIAG_EVENT_DEF(wlan_diag_event, struct vos_event_wlan_eapol);

    wlan_diag_event.event_sub_type = eapol_params.event_sub_type;
    wlan_diag_event.eapol_packet_type = eapol_params.eapol_packet_type;
    wlan_diag_event.eapol_key_info = eapol_params.eapol_key_info;
    wlan_diag_event.eapol_rate = eapol_params.eapol_rate;
    vos_mem_copy(wlan_diag_event.dest_addr,
                  eapol_params.dest_addr,
                  sizeof (wlan_diag_event.dest_addr));
    vos_mem_copy(wlan_diag_event.src_addr,
                 eapol_params.src_addr,
                 sizeof (wlan_diag_event.src_addr));
    WLAN_VOS_DIAG_EVENT_REPORT(&wlan_diag_event, EVENT_WLAN_EAPOL);
}

/*
 * wlan_hdd_log_eapol() - Function to check and extract EAPOL params
 * @skb:               skb data
 * @event_type:        One of enum wifi_connectivity_events to indicate Tx/Rx
 *
 * This function parses the input skb data to get the EAPOL params,if the
 * packet is EAPOL and store it in the pointer passed as input
 *
 * Return: None
 *
 */
void wlan_hdd_log_eapol(struct sk_buff *skb,
                               uint8_t event_type)
{
    struct vos_event_wlan_eapol eapol_params;

    wlan_hdd_get_eapol_params(skb, &eapol_params, event_type);
    wlan_hdd_event_eapol_log(eapol_params);
    VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
               "Eapol subtype is %d and key info is %d\n",
               eapol_params.event_sub_type,eapol_params.eapol_key_info);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
