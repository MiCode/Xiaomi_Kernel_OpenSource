/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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
         Copyright 2008 (c) Qualcomm, Incorporated.
         All Rights Reserved.
         Qualcomm Confidential and Proprietary.
  
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

#ifdef DEBUG_ROAM_DELAY
#include "vos_utils.h"
#endif

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
};

#define HDD_TX_TIMEOUT_RATELIMIT_INTERVAL 20*HZ
#define HDD_TX_TIMEOUT_RATELIMIT_BURST    1
#define HDD_TX_STALL_SSR_THRESHOLD        5
#define HDD_TX_STALL_RECOVERY_THRESHOLD HDD_TX_STALL_SSR_THRESHOLD - 2

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
   v_SIZE_t size = 0;
   v_U8_t skbStaIdx;
   skb_list_node_t *pktNode = NULL;
   hdd_list_node_t *tmp = NULL, *next = NULL;
   struct netdev_queue *txq;
   struct sk_buff *skb = NULL;

   if (NULL == pAdapter)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter is NULL %u"), STAId);
       VOS_ASSERT(0);
       return;
   }

   for (i = 0; i < NUM_TX_QUEUES; i++)
   {
      spin_lock_bh(&pAdapter->wmm_tx_queue[i].lock);

      if ( list_empty( &pAdapter->wmm_tx_queue[i].anchor ) )
      {
         spin_unlock_bh(&pAdapter->wmm_tx_queue[i].lock);
         continue;
      }

      /* Iterate through the queue and identify the data for STAId */
      list_for_each_safe(tmp, next, &pAdapter->wmm_tx_queue[i].anchor)
      {
         pktNode = list_entry(tmp, skb_list_node_t, anchor);
         if (pktNode != NULL)
         {
            skb = pktNode->skb;

            /* Get the STAId from data */
            skbStaIdx = *(v_U8_t *)(((v_U8_t *)(skb->data)) - 1);
            if (skbStaIdx == STAId)
            {
               /* Data for STAId is freed along with the queue node */
               list_del(tmp);
               kfree_skb(skb);

               ++pAdapter->hdd_stats.hddTxRxStats.txFlushed;
               ++pAdapter->hdd_stats.hddTxRxStats.txFlushedAC[i];
               pAdapter->wmm_tx_queue[i].count--;
            }
         }
      }

      /* Restart the queue only-if suspend and the queue was flushed */
      hdd_list_size( &pAdapter->wmm_tx_queue[i], &size );
      txq = netdev_get_tx_queue(pAdapter->dev, i);

      if (VOS_TRUE == pAdapter->isTxSuspended[i] &&
          size <= HDD_TX_QUEUE_LOW_WATER_MARK &&
          netif_tx_queue_stopped(txq) )
      {
         netif_tx_start_queue(txq);
         pAdapter->isTxSuspended[i] = VOS_FALSE;
         ++pAdapter->hdd_stats.hddTxRxStats.txDequeDePressured;
         ++pAdapter->hdd_stats.hddTxRxStats.txDequeDePressuredAC[i];
      }

      spin_unlock_bh(&pAdapter->wmm_tx_queue[i].lock);
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

   if (pAdapter == NULL)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
       FL("pAdapter is NULL"));
      VOS_ASSERT(0);
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

          WLANSAP_PopulateDelStaParams(hdr->addr1, eCsrForcedDeauthSta,
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
   return;

mgmt_handled:
   hdd_sendActionCnf( pAdapter, TRUE );
   return;
fail:
   kfree_skb(pAdapter->skb_to_tx);
   pAdapter->skb_to_tx = NULL;
   return;
}

void hdd_mon_tx_work_queue(struct work_struct *work)
{
   hdd_adapter_t* pAdapter = container_of(work, hdd_adapter_t, monTxWorkQueue);
   hdd_mon_tx_mgmt_pkt(pAdapter);
}

int hdd_mon_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
   v_U16_t rt_hdr_len;
   struct ieee80211_hdr *hdr;
   hdd_adapter_t *pPgBkAdapter, *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
   struct ieee80211_radiotap_header *rtap_hdr =
                        (struct ieee80211_radiotap_header *)skb->data;

   /*Supplicant sends the EAPOL packet on monitor interface*/
   pPgBkAdapter = pAdapter->sessionCtx.monitor.pAdapterForTx;    
   if(pPgBkAdapter == NULL)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
           "%s: No Adapter to piggy back. Dropping the pkt on monitor inf",
                                                                 __func__);
      goto fail; /* too short to be possibly valid */
   }
 
   /* check if toal skb length is greater then radio tab header length of not */
   if (unlikely(skb->len < sizeof(struct ieee80211_radiotap_header)))
      goto fail; /* too short to be possibly valid */
   
   /* check if radio tap header version is correct or not */
   if (unlikely(rtap_hdr->it_version))
      goto fail; /* only version 0 is supported */
 
   /*Strip off the radio tap header*/
   rt_hdr_len = ieee80211_get_radiotap_len(skb->data);
 
   /* check if skb length if greator then total radio tap header length ot not*/
   if (unlikely(skb->len < rt_hdr_len))
      goto fail;
 
   /* Update the trans_start for this netdev */  
   dev->trans_start = jiffies;
   /*
    * fix up the pointers accounting for the radiotap
    * header still being in there.
    */
   skb_set_mac_header(skb, rt_hdr_len);
   skb_set_network_header(skb, rt_hdr_len);
   skb_set_transport_header(skb, rt_hdr_len); 

   /* Pull rtap header out of the skb */
   skb_pull(skb, rt_hdr_len);
  
   /*Supplicant adds: radiotap Hdr + radiotap data + 80211 Header. So after 
    * radio tap header and 802.11 header starts 
    */
   hdr = (struct ieee80211_hdr *)skb->data;
 
   /* Send data frames through the normal Data path. In this path we will 
    * conver rcvd 802.11 packet to 802.3 packet */
   if ( (hdr->frame_control & HDD_FRAME_TYPE_MASK)  == HDD_FRAME_TYPE_DATA)
   { 
      v_U8_t da[6];
      v_U8_t sa[6];

      memcpy (da, hdr->addr1, VOS_MAC_ADDR_SIZE);
      memcpy (sa, hdr->addr2, VOS_MAC_ADDR_SIZE);
 
      /* Pull 802.11 MAC header */ 
      skb_pull(skb, HDD_80211_HEADER_LEN);
 
      if ( HDD_FRAME_SUBTYPE_QOSDATA == 
          (hdr->frame_control & HDD_FRAME_SUBTYPE_MASK))
      {
         skb_pull(skb, HDD_80211_HEADER_QOS_CTL);
      }

      /* Pull LLC header */ 
      skb_pull(skb, HDD_LLC_HDR_LEN);

      /* Create space for Ethernet header */ 
      skb_push(skb, HDD_MAC_HDR_SIZE*2);
      memcpy(&skb->data[0], da, HDD_MAC_HDR_SIZE);
      memcpy(&skb->data[HDD_DEST_ADDR_OFFSET], sa, HDD_MAC_HDR_SIZE);

      /* Only EAPOL Data packets are allowed through monitor interface */ 
      if (vos_be16_to_cpu(
         (*(unsigned short*)&skb->data[HDD_ETHERTYPE_802_1_X_FRAME_OFFSET]) ) 
                                                     != HDD_ETHERTYPE_802_1_X)
      {
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
           "%s: Not a Eapol packet. Drop this frame", __func__);
         //If not EAPOL frames, drop them.
         kfree_skb(skb);
         return NETDEV_TX_OK;
      }

      skb->protocol = htons(HDD_ETHERTYPE_802_1_X);
 
      hdd_hostapd_select_queue(pPgBkAdapter->dev, skb);
      return hdd_softap_hard_start_xmit( skb, pPgBkAdapter->dev );
   }
   else
   {
      VOS_STATUS status;
      WLANTL_ACEnumType ac = 0;
      skb_list_node_t *pktNode = NULL;
      v_SIZE_t pktListSize = 0;

      spin_lock(&pAdapter->wmm_tx_queue[ac].lock);
      //If we have already reached the max queue size, disable the TX queue
      if ( pAdapter->wmm_tx_queue[ac].count == pAdapter->wmm_tx_queue[ac].max_size)
      {
         /* We want to process one packet at a time, so lets disable all TX queues
           * and re-enable the queues once we get TX feedback for this packet */
         netif_tx_stop_all_queues(pAdapter->dev);
         pAdapter->isTxSuspended[ac] = VOS_TRUE;
         spin_unlock(&pAdapter->wmm_tx_queue[ac].lock);      
         return NETDEV_TX_BUSY;   
      }
      spin_unlock(&pAdapter->wmm_tx_queue[ac].lock);      

      //Use the skb->cb field to hold the list node information
      pktNode = (skb_list_node_t *)&skb->cb;

      //Stick the OS packet inside this node.
      pktNode->skb = skb;

      INIT_LIST_HEAD(&pktNode->anchor);

      //Insert the OS packet into the appropriate AC queue
      spin_lock(&pAdapter->wmm_tx_queue[ac].lock);
      status = hdd_list_insert_back_size( &pAdapter->wmm_tx_queue[ac],
                                          &pktNode->anchor, &pktListSize );
      spin_unlock(&pAdapter->wmm_tx_queue[ac].lock);

      if ( !VOS_IS_STATUS_SUCCESS( status ) )
      {
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s:Insert Tx queue failed. Pkt dropped", __func__);
         kfree_skb(skb);
         return NETDEV_TX_OK;
      }

      if ( pktListSize == 1 )
      {
         /* In this context we cannot acquire any mutex etc. And to transmit 
          * this packet we need to call SME API. So to take care of this we will
          * schedule a workqueue 
          */
         schedule_work(&pPgBkAdapter->monTxWorkQueue);
      }
      return NETDEV_TX_OK;
   }
 
fail:
   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,
           "%s: Packet Rcvd at Monitor interface is not proper,"
           " Dropping the packet",
            __func__);
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
   WLANTL_ACEnumType ac;
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

   ++pAdapter->hdd_stats.hddTxRxStats.txXmitCalled;

   if (unlikely(netif_subqueue_stopped(dev, skb))) {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                  "%s is called when netif TX %d is disabled",
                  __func__, skb->queue_mapping);
       return NETDEV_TX_BUSY;
   }

   //Get TL AC corresponding to Qdisc queue index/AC.
   ac = hdd_QdiscAcToTlAC[skb->queue_mapping];

   if (WLAN_HDD_IBSS == pAdapter->device_mode)
   {
      v_MACADDR_t *pDestMacAddress = (v_MACADDR_t*)skb->data;

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

      STAId = *(v_U8_t *)(((v_U8_t *)(skb->data)) - 1);

      if ((STAId == HDD_WLAN_INVALID_STA_ID) &&
          (vos_is_macaddr_broadcast( pDestMacAddress ) ||
           vos_is_macaddr_group(pDestMacAddress)))
      {
         STAId = IBSS_BROADCAST_STAID;
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_LOW,
                    "%s: BC/MC packet", __func__);
      }
      else if (STAId == HDD_WLAN_INVALID_STA_ID)
      {
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                    "%s: Received Unicast frame with invalid staID", __func__);
         ++pAdapter->stats.tx_dropped;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[ac];
         kfree_skb(skb);
         return NETDEV_TX_OK;
      }
   }
   else
   {
      STAId = pHddStaCtx->conn_info.staId[0];
   }

   //user priority from IP header, which is already extracted and set from
   //select_queue call back function
   up = skb->priority;

   ++pAdapter->hdd_stats.hddTxRxStats.txXmitClassifiedAC[ac];

#ifdef HDD_WMM_DEBUG
   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
              "%s: Classified as ac %d up %d", __func__, ac, up);
#endif // HDD_WMM_DEBUG

#ifdef DEBUG_ROAM_DELAY
   vos_record_roam_event(e_HDD_FIRST_XMIT_TIME, (void *)skb, 0);
   //Should we check below global to avoid function call each time ??
/*
   if(gRoamDelayMetaInfo.hdd_monitor_tx)
   {
   }
 */
#endif

   spin_lock(&pAdapter->wmm_tx_queue[ac].lock);
   /*CR 463598,384996*/
   /*For every increment of 10 pkts in the queue, we inform TL about pending pkts.
    *We check for +1 in the logic,to take care of Zero count which
    *occurs very frequently in low traffic cases */
   if((pAdapter->wmm_tx_queue[ac].count + 1) % 10 == 0)
   {
      /* Use the following debug statement during Engineering Debugging.There are chance that this will lead to a Watchdog Bark
            * if it is in the mainline code and if the log level is enabled by someone for debugging
           VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,"%s:Queue is Filling up.Inform TL again about pending packets", __func__);*/

      status = WLANTL_STAPktPending( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                    STAId, ac
                                    );
      if ( !VOS_IS_STATUS_SUCCESS( status ) )
      {
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                    "%s: WLANTL_STAPktPending() returned error code %d",
                    __func__, status);
         ++pAdapter->stats.tx_dropped;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[ac];
         kfree_skb(skb);
         spin_unlock(&pAdapter->wmm_tx_queue[ac].lock);
         return NETDEV_TX_OK;
      }
   }
   //If we have already reached the max queue size, disable the TX queue
   if ( pAdapter->wmm_tx_queue[ac].count == pAdapter->wmm_tx_queue[ac].max_size)
   {
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitBackPressured;
         ++pAdapter->hdd_stats.hddTxRxStats.txXmitBackPressuredAC[ac];
         netif_tx_stop_queue(netdev_get_tx_queue(dev, skb_get_queue_mapping(skb)));
         pAdapter->isTxSuspended[ac] = VOS_TRUE;
         txSuspended = VOS_TRUE;
   }

   /* If 3/4th of the max queue size is used then enable the flag.
    * This flag indicates to place the DHCP packets in VOICE AC queue.*/
   if (WLANTL_AC_BE == ac)
   {
      if (pAdapter->wmm_tx_queue[ac].count >= HDD_TX_QUEUE_LOW_WATER_MARK)
      {
          pAdapter->isVosLowResource = VOS_TRUE;
      }
      else
      {
          pAdapter->isVosLowResource = VOS_FALSE;
      }
   }

   spin_unlock(&pAdapter->wmm_tx_queue[ac].lock);

   if ( ( NULL != pHddCtx ) && pHddCtx->cfg_ini->enableDhcpDebug )
   {
       hdd_dump_dhcp_pkt(skb, TX_PATH);
   }

   if (VOS_TRUE == txSuspended)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                  "%s: TX queue full for AC=%d Disable OS TX queue",
                  __func__, ac );
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
   spin_lock(&pAdapter->wmm_tx_queue[ac].lock);
   status = hdd_list_insert_back_size( &pAdapter->wmm_tx_queue[ac], &pktNode->anchor, &pktListSize );
   spin_unlock(&pAdapter->wmm_tx_queue[ac].lock);

   if ( !VOS_IS_STATUS_SUCCESS( status ) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,"%s:Insert Tx queue failed. Pkt dropped", __func__);
      ++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;
      ++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[ac];
      ++pAdapter->stats.tx_dropped;
      kfree_skb(skb);
      return NETDEV_TX_OK;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.txXmitQueued;
   ++pAdapter->hdd_stats.hddTxRxStats.txXmitQueuedAC[ac];
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
   if ( granted && ( pktListSize == 1 ))
   {
      //Let TL know we have a packet to send for this AC
      //VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s:Indicating Packet to TL", __func__);
      status = WLANTL_STAPktPending(
                                  (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                   STAId, ac );
      if ( !VOS_IS_STATUS_SUCCESS( status ) )
      {
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN, "%s: Failed to signal TL for AC=%d", __func__, ac );

         //Remove the packet from queue. It must be at the back of the queue, as TX thread cannot preempt us in the middle
         //as we are in a soft irq context. Also it must be the same packet that we just allocated.
         spin_lock(&pAdapter->wmm_tx_queue[ac].lock);
         status = hdd_list_remove_back( &pAdapter->wmm_tx_queue[ac], &anchor );
         spin_unlock(&pAdapter->wmm_tx_queue[ac].lock);
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
   struct netdev_queue *txq;
   int i = 0;
   v_ULONG_t diff_in_jiffies = 0;

   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
      "%s: Transmission timeout occurred", __func__);

   if ( NULL == pAdapter )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter is NULL"));
      VOS_ASSERT(0);
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

   for (i = 0; i < 8; i++)
   {
      txq = netdev_get_tx_queue(dev, i);
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                "Queue%d status: %d", i, netif_tx_queue_stopped(txq));
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

   if (pAdapter->hdd_stats.hddTxRxStats.continuousTxTimeoutCount ==
          HDD_TX_STALL_RECOVERY_THRESHOLD)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: Request firmware for recovery",__func__);
      WLANTL_TLDebugMessage(WLANTL_DEBUG_FW_CLEANUP);
   }

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
  @brief hdd_stats() - Function registered with the Linux OS for 
  device TX/RX statistic

  @param dev      : [in] pointer to Libra network device
  
  @return         : pointer to net_device_stats structure
  ===========================================================================*/
struct net_device_stats* hdd_stats(struct net_device *dev)
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
   if (pAdapter == NULL)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                              "%s: HDD adapter context is Null", __func__);
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
              FL("pAdapter is NULL %u"), *pStaId);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.txFetched;

   *ppVosPacket = NULL;

   //Make sure the AC being asked for is sane
   if( ac >= WLANTL_MAX_AC || ac < 0)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                            "%s: Invalid AC %d passed by TL", __func__, ac);
      return VOS_STATUS_E_FAILURE;
   }

   ++pAdapter->hdd_stats.hddTxRxStats.txFetchedAC[ac];

#ifdef HDD_WMM_DEBUG
   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
                              "%s: AC %d passed by TL", __func__, ac);
#endif // HDD_WMM_DEBUG

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
   }
   else
   {
      ++pAdapter->hdd_stats.hddTxRxStats.txFetchDequeueError;
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN, "%s: Error in de-queuing "
         "skb from Tx queue status = %d", __func__, status );
      vos_pkt_return_packet(pVosPacket);
      return VOS_STATUS_E_FAILURE;
   }
   /* In IBSS the staID can change and hdd uses the same queue for all staid.
    * Due to this the fetch may be called for staID x, but data might be of
    * staID y. So better get the staId again from SKB and use this to process
    * the packet further.
    */
   if (WLAN_HDD_IBSS == pAdapter->device_mode)
   {
      *pStaId = *(v_U8_t *)(((v_U8_t *)(skb->data)) - 1);

      /* If HDD_WLAN_INVALID_STA_ID no need to check the destination,
       * just make the staID 0. As if HDD_WLAN_INVALID_STA_ID and
       * destination is not BC of MC, the packet is already dropped in
       * hdd_hard_start_xmit.
       */
      if (*pStaId == HDD_WLAN_INVALID_STA_ID)
         *pStaId = IBSS_BROADCAST_STAID;
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
   {
       v_MACADDR_t *pDestMacAddress = (v_MACADDR_t*)skb->data;
       /* vos_is_macaddr_group expects data in v_MACADDR_t format
        */
       if (vos_is_macaddr_group(pDestMacAddress))
       {
            pAdapter->hdd_stats.hddTxRxStats.txMcast[ac]++;
       }

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

   if (pHddCtx->cfg_ini->gEnableDebugLog)
   {
      proto_type = vos_pkt_get_proto_type(skb,
                                          pHddCtx->cfg_ini->gEnableDebugLog);
      if (VOS_PKT_PROTO_TYPE_EAPOL & proto_type)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                   "STA TX EAPOL");
      }
      else if (VOS_PKT_PROTO_TYPE_DHCP & proto_type)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                   "STA TX DHCP");
      }
   }

   vos_pkt_get_packet_length( pVosPacket,&packet_size );
   if( HDD_ETHERTYPE_ARP_SIZE == packet_size )
      pPktMetaInfo->ucIsArp = hdd_IsARP( pVosPacket ) ? 1 : 0;

#ifdef FEATURE_WLAN_WAPI
   // Override usIsEapol value when its zero for WAPI case
      pPktMetaInfo->ucIsWai = hdd_IsWAIPacket( pVosPacket ) ? 1 : 0;
#endif /* FEATURE_WLAN_WAPI */

   if ((HDD_WMM_USER_MODE_NO_QOS == pHddCtx->cfg_ini->WmmMode) ||
       (!pAdapter->hddWmmStatus.wmmQap))
   {
      // either we don't want QoS or the AP doesn't support QoS
      pPktMetaInfo->ucUP = 0;
      pPktMetaInfo->ucTID = 0;
   }
   else
   {
      /* 1. Check if ACM is set for this AC 
       * 2. If set, check if this AC had already admitted 
       * 3. If not already admitted, downgrade the UP to next best UP
       * 4. Allow only when medium time is non zero when Addts accepted else downgrade traffic.
            we opted downgrading over Delts when medium time is zero because while doing downgradig
            driver is not clearing the wmm context so consider in subsequent roaming if AP (new or
            same AP) accept the Addts with valid medium time no application support is required
            where if we have opted delts Applications have to again do Addts or STA will never
            go for Addts.*/

      if(!pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessRequired ||
         (pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcTspecValid &&
          pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcTspecInfo.medium_time))
      {
        pPktMetaInfo->ucUP = pktNode->userPriority;
        pPktMetaInfo->ucTID = pPktMetaInfo->ucUP;
      }
      else
      {
        //Downgrade the UP
        acAdmitted = pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcTspecValid;
        newAc = WLANTL_AC_BK;
        for (i=ac-1; i>0; i--)
        {
            if (pAdapter->hddWmmStatus.wmmAcStatus[i].wmmAcAccessRequired == 0)
            {
                newAc = i;
                break;
            }
        }
        pPktMetaInfo->ucUP = hddWmmAcToHighestUp[newAc];
        pPktMetaInfo->ucTID = pPktMetaInfo->ucUP;
        VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_LOW,"Downgrading UP %d to UP %d ", pktNode->userPriority, pPktMetaInfo->ucUP);
      }
   }

#ifdef DEBUG_ROAM_DELAY
   vos_record_roam_event(e_TL_FIRST_XMIT_TIME, NULL, 0);
#endif

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
   
   if (NULL == pAdapter)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                            "%s: pAdapter is Null", __func__);
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
   if( (NULL == pAdapter)  || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              "%s: pAdapter is Null or adapter has invalid magic for staId %u",
                 __func__, staId);
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
      // Tell VOS to detach the OS packet from the VOS packet
      status = vos_pkt_get_os_packet( pVosPacket, (v_VOID_t **)&skb, VOS_TRUE );
      if(!VOS_IS_STATUS_SUCCESS( status ))
      {
         ++pAdapter->hdd_stats.hddTxRxStats.rxDropped;
         VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
                                "%s: Failure extracting skb from vos pkt", __func__);
         return VOS_STATUS_E_FAILURE;
      }

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

        wlan_hdd_tdls_extract_sa(skb, mac);

        if (vos_is_macaddr_group((v_MACADDR_t *)mac)) {
            VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_MED,
                      "rx broadcast packet, not adding to peer list");
        } else if (memcmp(pHddStaCtx->conn_info.bssId,
                            mac, 6) != 0) {
            hddTdlsPeer_t *curr_peer;
            VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_MED,
                      "rx extract mac:" MAC_ADDRESS_STR,
                      MAC_ADDR_ARRAY(mac) );
            curr_peer = wlan_hdd_tdls_find_peer(pAdapter, mac, TRUE);
            if ((NULL != curr_peer) && (eTDLS_LINK_CONNECTED == curr_peer->link_status)
                 && (TRUE == pRxMetaInfo->isStaTdls))
            {
                wlan_hdd_tdls_increment_pkt_count(pAdapter, mac, 0);
                VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,"rssi is %d", pRxMetaInfo->rssiAvg);
                wlan_hdd_tdls_set_rssi (pAdapter, mac, pRxMetaInfo->rssiAvg);
            }
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
            VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                      "STA RX EAPOL");
         }
         else if (VOS_PKT_PROTO_TYPE_DHCP & proto_type)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                      "STA RX DHCP");
         }
      }

#ifdef DEBUG_ROAM_DELAY
      vos_record_roam_event(e_HDD_RX_PKT_CBK_TIME, (void *)skb, 0);
      //Should we check below global to avoid function call each time ??
      /*
         if(gRoamDelayMetaInfo.hdd_monitor_rx)
         {
         }
       */
#endif
      if ( pHddCtx->cfg_ini->enableDhcpDebug )
      {
          hdd_dump_dhcp_pkt(skb, RX_PATH);
      }

      skb->dev = pAdapter->dev;
      skb->protocol = eth_type_trans(skb, skb->dev);
      skb->ip_summed = CHECKSUM_NONE;
      ++pAdapter->hdd_stats.hddTxRxStats.rxPackets;
      ++pAdapter->stats.rx_packets;
      pAdapter->stats.rx_bytes += skb->len;
#ifdef WLAN_OPEN_SOURCE
#ifdef WLAN_FEATURE_HOLD_RX_WAKELOCK
      wake_lock_timeout(&pHddCtx->rx_wake_lock, msecs_to_jiffies(HDD_WAKE_LOCK_DURATION));
#endif
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

   if (NULL == phddctx)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("phddctx is NULL"));
       VOS_ASSERT(0);
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

        if ( pAdapter )
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
                for (staId = 0; staId < WLAN_MAX_STA_COUNT; staId++)
                {
                    if ((pAdapter->aStaInfo[staId].isUsed) &&
                        (WLANTL_STA_AUTHENTICATED ==
                                          pAdapter->aStaInfo[staId].tlSTAState))
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
    return;
}

