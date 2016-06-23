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

/**========================================================================

  \file  bap_hdd_main.c

  \brief 802.11 BT-AMP PAL Host Device Driver implementation

   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

  ========================================================================*/

/**=========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header: /prj/qct/asw/engbuilds/scl/users02/jzmuda/gb-bluez/vendor/qcom/proprietary/wlan/libra/CORE/HDD/src/bap_hdd_main.c,v 1.63 2011/04/01 15:24:20 jzmuda Exp jzmuda $   $DateTime: $ $Author: jzmuda $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  12/1/09     JZmuda    Created module.

  ==========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#ifdef WLAN_BTAMP_FEATURE
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/spinlock.h>
//#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/io.h>
//#include <wlan_qct_driver.h>
#include <wlan_hdd_includes.h>
#include <wlan_hdd_dp_utils.h>
/* -------------------------------------------------------------------------*/
#include <bap_hdd_main.h>
#include <vos_api.h>
#include <bapApi.h>
#include <btampHCI.h>
/* -------------------------------------------------------------------------*/
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include <wlan_hdd_misc.h>
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

// the difference between the next two is that the first is the max
// number we support in our current implementation while the second is
// the max allowed by the spec
#define BSL_MAX_PHY_LINKS           ( BSL_MAX_CLIENTS * BSL_MAX_PHY_LINK_PER_CLIENT )
#define BSL_MAX_ALLOWED_PHY_LINKS   255

// these likely will need tuning based on experiments
#define BSL_MAX_RX_PKT_DESCRIPTOR   100
#define BSL_MAX_TX_PKT_DESCRIPTOR   100

// these caps are in place to not have run-away queues, again needs empirical tuning
#define BSL_MAX_SIZE_TX_ACL_QUEUE   50
#define BSL_MAX_SIZE_RX_ACL_QUEUE   50
#define BSL_MAX_SIZE_RX_EVT_QUEUE   50

#if 0
What are the maximum sizes of a command packet, an event packet and an ACL
data packet?

[JimZ]: Sizes:
1. Cmd Maximum size is slightly greater than 672 btyes.  But I am pretty sure
right now that I will never have more than 240 bytes to send down at a time.  And
that is good. Because some rather unpleasant things happen at the HCI interface
if I exceed that.  ( Think 8-bit CPUs.  And the limitations of an 8-bit length
                    field. )

2. Event -  Ditto.

3. Data 1492 bytes
#endif

// jimz
// TLV related defines

#define USE_FINAL_FRAMESC
//#undef USE_FINAL_FRAMESC
// jimz
// TLV related defines

#ifndef USE_FINAL_FRAMESC        //USE_FINAL_FRAMESC
// AMP ASSOC TLV related defines
#define AMP_ASSOC_TLV_TYPE_SIZE 2
#define AMP_ASSOC_TLV_LEN_SIZE 2
#define AMP_ASSOC_TLV_TYPE_AND_LEN_SIZE  (AMP_ASSOC_TLV_TYPE_SIZE + AMP_ASSOC_TLV_LEN_SIZE)

// FLOW SPEC TLV related defines
#define FLOWSPEC_TYPE_SIZE 2
#define FLOWSPEC_LEN_SIZE 2
#define FLOWSPEC_TYPE_AND_LEN_SIZE  (FLOWSPEC_TYPE_SIZE + FLOWSPEC_LEN_SIZE)

// CMD TLV related defines
#define CMD_TLV_TYPE_SIZE 2
#define CMD_TLV_LEN_SIZE 2
#define CMD_TLV_TYPE_AND_LEN_SIZE  (CMD_TLV_TYPE_SIZE + CMD_TLV_LEN_SIZE)

// Event TLV related defines
#define EVENT_TLV_TYPE_SIZE 2
#define EVENT_TLV_LEN_SIZE 2
#define EVENT_TLV_TYPE_AND_LEN_SIZE  (EVENT_TLV_TYPE_SIZE + EVENT_TLV_LEN_SIZE)

// Data header size related defines
#define DATA_HEADER_SIZE 4

#else                            //USE_FINAL_FRAMESC

// AMP ASSOC TLV related defines
#define AMP_ASSOC_TLV_TYPE_SIZE 1
#define AMP_ASSOC_TLV_LEN_SIZE 2
#define AMP_ASSOC_TLV_TYPE_AND_LEN_SIZE  (AMP_ASSOC_TLV_TYPE_SIZE + AMP_ASSOC_TLV_LEN_SIZE)

// FLOW SPEC TLV related defines
#define FLOWSPEC_TYPE_SIZE 1
#define FLOWSPEC_LEN_SIZE 1
#define FLOWSPEC_TYPE_AND_LEN_SIZE  (FLOWSPEC_TYPE_SIZE + FLOWSPEC_LEN_SIZE)

// CMD TLV related defines
#define CMD_TLV_TYPE_SIZE 2
#define CMD_TLV_LEN_SIZE 1
#define CMD_TLV_TYPE_AND_LEN_SIZE  (CMD_TLV_TYPE_SIZE + CMD_TLV_LEN_SIZE)

// Event TLV related defines
#define EVENT_TLV_TYPE_SIZE 1
#define EVENT_TLV_LEN_SIZE 1
#define EVENT_TLV_TYPE_AND_LEN_SIZE  (EVENT_TLV_TYPE_SIZE + EVENT_TLV_LEN_SIZE)

// Data header size related defines
#define DATA_HEADER_SIZE 4

#endif                           // USE_FINAL_FRAMESC
// jimz

#define BSL_MAX_EVENT_SIZE 700

#define BSL_DEV_HANDLE 0x1234

// Debug related defines
#define DBGLOG printf
//#define DUMPLOG
#if defined DUMPLOG
#define DUMPLOG(n, name1, name2, aStr, size) do {                       \
        int i;                                                          \
        DBGLOG("%d. %s: %s = \n", n, name1, name2);                     \
        for (i = 0; i < size; i++)                                      \
            DBGLOG("%2.2x%s", ((unsigned char *)aStr)[i], i % 16 == 15 ? "\n" : " "); \
        DBGLOG("\n");                                                   \
    } while (0)
#else
#define DUMPLOG(n, name1, name2, aStr, size)
#endif

// These are required to replace some Microsoft specific specifiers
//#define UNALIGNED __align
#define UNALIGNED
#define INFINITE 0

#define BT_AMP_HCI_CTX_MAGIC 0x48434949    // "HCII"

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

// Temporary Windows types
typedef int            BOOL;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned long  DWORD;
typedef void * HANDLE;
typedef char  TCHAR;
typedef void *LPVOID;
typedef const void *LPCVOID;

typedef struct
{
    BOOL                         used;          // is this a valid context?
    vos_event_t                  ReadableEvt;   // the event a ReadFile can block on
    ptBtampHandle                bapHdl;        // our handle in BAP
    vos_list_t                   PhyLinks;      // a list of all associations setup by this client
//  Newly added for BlueZ
    struct hci_dev               *hdev;        // the BlueZ HCI device structure

    /* I don't know how many of these Tx fields we need */
    spinlock_t                   lock;         /* For serializing operations */

    struct                       sk_buff_head txq; /* We need the ACL Data Tx queue */

    /* We definitely need some of these rx_skb fields */
    unsigned long                rx_state;
    unsigned long                rx_count;
    struct sk_buff               *rx_skb;

    struct net_device            *p_dev; // Our parent wlan network device

} BslClientCtxType;

typedef struct
{
    BslClientCtxType* pctx;
    /* Tx skb queue and the workstructure for handling Tx as deferred work. */
    struct sk_buff               *tx_skb;

    struct work_struct           hciInterfaceProcessing;
    v_U32_t                      magic;

} BslHciWorkStructure;

typedef struct
{
    TCHAR* ValueName;     // name of the value
    DWORD  Type;          // type of value
    DWORD  DwordValue;    // DWORD value
    TCHAR* StringValue;   // string value

} BslRegEntry;

typedef struct
{
    BOOL              used;                // is this a valid context?
    hdd_list_t        ACLTxQueue[WLANTL_MAX_AC];  // the TX ACL queues
    BslClientCtxType* pClientCtx;          // ptr to application context that spawned
    // this association
    v_U8_t            PhyLinkHdl;          // BAP handle for this association
    void*             pPhyLinkDescNode;    // ptr to node in list of assoc in client ctx
    // real type BslPhyLinksNodeType*

} BslPhyLinkCtxType;

typedef struct
{
    vos_list_node_t    node;  // MUST be first element
    BslPhyLinkCtxType* pPhy;  // ptr to an association context

} BslPhyLinksNodeType;

typedef struct
{
    vos_list_node_t node;     // MUST be first element
    vos_pkt_t*      pVosPkt;  // ptr to a RX VoS pkt which can hold an HCI event or ACL data

} BslRxListNodeType;

// Borrowed from wlan_hdd_dp_utils.h
typedef struct
{
    hdd_list_node_t     node;         // MUST be first element
    struct sk_buff *    skb;          // ptr to the ACL data

} BslTxListNodeType;

typedef struct
{
    BslPhyLinkCtxType* ptr;   // ptr to the association context for this phy_link_handle

} BslPhyLinkMapEntryType;

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/
BslClientCtxType* gpBslctx;

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/
// Temporary (until multi-phy link) pointer to BT-AMP context
static void *gpCtx;

// an efficient lookup from phy_link_handle to phy link context
static BslPhyLinkMapEntryType BslPhyLinkMap[BSL_MAX_ALLOWED_PHY_LINKS];

//static HANDLE hBsl = NULL; //INVALID_HANDLE_VALUE;
static BOOL bBslInited = FALSE;

static BslClientCtxType BslClientCtx[BSL_MAX_CLIENTS];
//static vos_lock_t BslClientLock;

static BslPhyLinkCtxType BslPhyLinkCtx[BSL_MAX_PHY_LINKS];
//static vos_lock_t BslPhyLock;

// the pool for association contexts
static vos_list_t BslPhyLinksDescPool;
static BslPhyLinksNodeType BslPhyLinksDesc[BSL_MAX_PHY_LINKS];

//static v_U32_t Eventlen = 0;

/*---------------------------------------------------------------------------
 *   Forward declarations
 *-------------------------------------------------------------------------*/
static void bslWriteFinish(struct work_struct *work);

/*---------------------------------------------------------------------------
 *   Driver Entry points and Structure definitions
 *-------------------------------------------------------------------------*/
static int BSL_Open (struct hci_dev *hdev);
static int BSL_Close (struct hci_dev *hdev);
static int BSL_Flush(struct hci_dev *hdev);
static int BSL_IOControl(struct hci_dev *hdev, unsigned int cmd, unsigned long arg);
static int BSL_Write(struct sk_buff *skb);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static void BSL_Destruct(struct hci_dev *hdev);
#endif


/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/
static v_BOOL_t WLANBAP_AmpConnectionAllowed(void)
{
    v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
    hdd_context_t *pHddCtx;
    v_BOOL_t retVal = VOS_FALSE;

    if (NULL != pVosContext)
    {
       pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
       if (NULL != pHddCtx)
       {
           return pHddCtx->isAmpAllowed;
       }
       else
       {
           return retVal;
       }
    }
    return retVal;
}

/**
  @brief WLANBAP_STAFetchPktCB() - The fetch packet callback registered
  with BAP by HDD.

  It is called by the BAP immediately upon the underlying
  WLANTL_STAFetchPktCBType routine being called.  Which is called by
  TL when the scheduling algorithms allows for transmission of another
  packet to the module.

  This function is here to "wrap" or abstract WLANTL_STAFetchPktCBType.
  Because the BAP-specific HDD "shim" layer (BSL) doesn't know anything
  about STAIds, or other parameters required by TL.

  @param pHddHdl: [in] The HDD(BSL) specific context for this association.
  Use the STAId passed to me by TL in WLANTL_STAFetchCBType to retreive
  this value.
  @param  pucAC: [inout] access category requested by TL, if HDD does not
  have packets on this AC it can choose to service another AC queue in
  the order of priority
  @param  vosDataBuff: [out] pointer to the VOSS data buffer that was
  transmitted
  @param tlMetaInfo: [out] meta info related to the data frame

  @return
  The result code associated with performing the operation
*/
static VOS_STATUS WLANBAP_STAFetchPktCB
(
    v_PVOID_t             pHddHdl,
    WLANTL_ACEnumType     ucAC,
    vos_pkt_t**           vosDataBuff,
    WLANTL_MetaInfoType*  tlMetaInfo
)
{
    BslPhyLinkCtxType* pPhyCtx;
    VOS_STATUS VosStatus;
    v_U8_t AcIdxStart;
    v_U8_t AcIdx;
    hdd_list_node_t *pLink;
    BslTxListNodeType *pNode;
    struct sk_buff *    skb;
    BslClientCtxType* pctx;
    WLANTL_ACEnumType Ac;
    vos_pkt_t* pVosPkt;
    WLANTL_MetaInfoType TlMetaInfo;
    pctx = &BslClientCtx[0];

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_LOW, "WLANBAP_STAFetchPktCB" );

    // sanity checking
    if( pHddHdl == NULL || vosDataBuff == NULL ||
            tlMetaInfo == NULL || ucAC >= WLANTL_MAX_AC || ucAC < 0 )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_STAFetchPktCB bad input" );
        return VOS_STATUS_E_FAILURE;
    }

    // Initialize the VOSS packet returned to NULL - in case of error
    *vosDataBuff = NULL;

    pPhyCtx = (BslPhyLinkCtxType *)pHddHdl;
    AcIdx = AcIdxStart = ucAC;

    spin_lock_bh(&pPhyCtx->ACLTxQueue[AcIdx].lock);
    VosStatus = hdd_list_remove_front( &pPhyCtx->ACLTxQueue[AcIdx], &pLink );
    spin_unlock_bh(&pPhyCtx->ACLTxQueue[AcIdx].lock);

    if ( VOS_STATUS_E_EMPTY == VosStatus )
    {
        do
        {
            AcIdx = (AcIdx + 1) % WLANTL_MAX_AC;

            spin_lock_bh(&pPhyCtx->ACLTxQueue[AcIdx].lock);
            VosStatus = hdd_list_remove_front( &pPhyCtx->ACLTxQueue[AcIdx], &pLink );
            spin_unlock_bh(&pPhyCtx->ACLTxQueue[AcIdx].lock);

        }
        while ( VosStatus == VOS_STATUS_E_EMPTY && AcIdx != AcIdxStart );

        if ( VosStatus == VOS_STATUS_E_EMPTY )
        {
            // Queue is empty.  This can happen.  Just return NULL back to TL...
            return(VOS_STATUS_E_EMPTY);
        }
        else if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_ASSERT( 0 );
        }
    }

    if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
    {
        VOS_ASSERT( 0 );
    }

    pNode = (BslTxListNodeType *)pLink;
    skb   = pNode->skb;

   // I will access the skb in a VOSS packet
   // Wrap the OS provided skb in a VOSS packet
    // Attach skb to VOS packet.
    VosStatus = vos_pkt_wrap_data_packet( &pVosPkt,
                                          VOS_PKT_TYPE_TX_802_3_DATA,
                                          skb,
                                          NULL,
                                          NULL);

    if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_STAFetchPktCB vos_pkt_wrap_data_packet "
             "failed status =%d", VosStatus );
        kfree_skb(skb);  
        return VosStatus;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO, "%s: pVosPkt(vos_pkt_t *)=%p", __func__,
               pVosPkt );

    VosStatus = WLANBAP_XlateTxDataPkt( pctx->bapHdl, pPhyCtx->PhyLinkHdl,
                                        &Ac, &TlMetaInfo, pVosPkt);

    if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_STAFetchPktCB WLANBAP_XlateTxDataPkt "
             "failed status =%d", VosStatus );

        // return the packet
        VosStatus = vos_pkt_return_packet( pVosPkt );
        kfree_skb(skb);  
        VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

        return VosStatus;
    }
    // give TL the VoS pkt
    *vosDataBuff = pVosPkt;

    // provide the meta-info BAP provided previously
    *tlMetaInfo = TlMetaInfo;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: *vosDataBuff(vos_pkt_t *)=%p", __func__, *vosDataBuff );

    return(VOS_STATUS_SUCCESS);
} // WLANBAP_STAFetchPktCB()

/**
  @brief WLANBAP_STARxCB() - The receive callback registered with BAP by HDD.

  It is called by the BAP immediately upon the underlying
  WLANTL_STARxCBType routine being called.  Which is called by
  TL to notify when a packet was received for a registered STA.

  @param  pHddHdl: [in] The HDD(BSL) specific context for this association.
  Use the STAId passed to me by TL in WLANTL_STARxCBType to retrieve this value.
  @param  vosDataBuff: [in] pointer to the VOSS data buffer that was received
  (it may be a linked list)
  @param  pRxMetaInfo: [in] Rx meta info related to the data frame

  @return
  The result code associated with performing the operation
*/
static VOS_STATUS WLANBAP_STARxCB
(
    v_PVOID_t              pHddHdl,
    vos_pkt_t*             vosDataBuff,
    WLANTL_RxMetaInfoType* pRxMetaInfo
)
{
    BslPhyLinkCtxType* pctx;
    BslClientCtxType* ppctx;
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    WLANTL_ACEnumType Ac; // this is not needed really
    struct sk_buff *skb = NULL;
    vos_pkt_t* pVosPacket;
    vos_pkt_t* pNextVosPacket;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_LOW, "WLANBAP_STARxCB" );

    // sanity checking
    if ( pHddHdl == NULL || vosDataBuff == NULL || pRxMetaInfo == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_STARxCB bad input" );
        if(NULL != vosDataBuff)
        {
            VosStatus = vos_pkt_return_packet( vosDataBuff );
        }
        return VOS_STATUS_E_FAILURE;
    }

    pctx = (BslPhyLinkCtxType *)pHddHdl;
    ppctx = pctx->pClientCtx;

    if( NULL == ppctx )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_STARxCB ClientCtx is NULL" );
        VosStatus = vos_pkt_return_packet( vosDataBuff );
        return VOS_STATUS_E_FAILURE;
    }

    // walk the chain until all are processed
   pVosPacket = vosDataBuff;
   do
   {
       // get the pointer to the next packet in the chain
       // (but don't unlink the packet since we free the entire chain later)
       VosStatus = vos_pkt_walk_packet_chain( pVosPacket, &pNextVosPacket, VOS_FALSE);
       
       // both "success" and "empty" are acceptable results
       if (!((VosStatus == VOS_STATUS_SUCCESS) || (VosStatus == VOS_STATUS_E_EMPTY)))
       {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,"%s: Failure walking packet chain", __func__);
           return VOS_STATUS_E_FAILURE;
       }
       
       // process the packet
       VosStatus = WLANBAP_XlateRxDataPkt( ppctx->bapHdl, pctx->PhyLinkHdl,
                                              &Ac, pVosPacket );

       if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
       {
           VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_FATAL, "WLANBAP_STARxCB WLANBAP_XlateRxDataPkt "
           "failed status = %d", VosStatus );

           VosStatus = VOS_STATUS_E_FAILURE;

           break;
       }

       // Extract the OS packet (skb).
       // Tell VOS to detach the OS packet from the VOS packet
       VosStatus = vos_pkt_get_os_packet( pVosPacket, (v_VOID_t **)&skb, VOS_TRUE );
       if(!VOS_IS_STATUS_SUCCESS( VosStatus ))
       {
           VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "%s: Failure extracting skb from vos pkt. "
             "VosStatus = %d", __func__, VosStatus );

           VosStatus = VOS_STATUS_E_FAILURE;

           break;
       }

       //JEZ100809: While an skb is being handled by the kernel, is "skb->dev" de-ref'd?
       skb->dev = (struct net_device *) gpBslctx->hdev;
       bt_cb(skb)->pkt_type = HCI_ACLDATA_PKT;
       //skb->protocol = eth_type_trans(skb, skb->dev);
       //skb->ip_summed = CHECKSUM_UNNECESSARY;
 
       // This is my receive skb pointer
       gpBslctx->rx_skb = skb;

       // This is how data and events are passed up to BlueZ
       hci_recv_frame(gpBslctx->rx_skb);

       // now process the next packet in the chain
       pVosPacket = pNextVosPacket;
       
   } while (pVosPacket);


    //JEZ100922: We are free to return the enclosing VOSS packet.
    VosStatus = vos_pkt_return_packet( vosDataBuff );
    VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));


    return(VOS_STATUS_SUCCESS);
} // WLANBAP_STARxCB()

/**
  @brief WLANBAP_TxCompCB() - The Tx complete callback registered with BAP by HDD.

  It is called by the BAP immediately upon the underlying
  WLANTL_TxCompCBType routine being called.  Which is called by
  TL to notify when a transmission for a packet has ended.

  @param pHddHdl: [in] The HDD(BSL) specific context for this association
  @param vosDataBuff: [in] pointer to the VOSS data buffer that was transmitted
  @param wTxSTAtus: [in] status of the transmission

  @return
  The result code associated with performing the operation
*/
extern v_VOID_t WLANBAP_TxPacketMonitorHandler ( v_PVOID_t ); // our handle in BAP

static VOS_STATUS WLANBAP_TxCompCB
(
    v_PVOID_t      pHddHdl,
    vos_pkt_t*     vosDataBuff,
    VOS_STATUS     wTxSTAtus
)
{
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    //BslTxListNodeType* pTxNode;
    void* pOsPkt = NULL;
    BslPhyLinkCtxType* pctx;
    BslClientCtxType* ppctx;
    static int num_packets;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO, "WLANBAP_TxCompCB. vosDataBuff(vos_pkt_t *)=%p", vosDataBuff );

    // be aware that pHddHdl can be NULL or can point to the per association
    // BSL context from the register data plane. In either case it does not
    // matter since we will simply free the VoS pkt and reclaim the TX
    // descriptor

    // sanity checking
    if ( vosDataBuff == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_TxCompCB bad input" );
        return VOS_STATUS_E_FAILURE;
    }

    //Return the skb to the OS
    VosStatus = vos_pkt_get_os_packet( vosDataBuff, &pOsPkt, VOS_TRUE );
    if(!VOS_IS_STATUS_SUCCESS( VosStatus ))
    {
        //This is bad but still try to free the VOSS resources if we can
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,"%s: Failure extracting skb from vos pkt", __func__);
        vos_pkt_return_packet( vosDataBuff );
        return VOS_STATUS_E_FAILURE;
    }

    kfree_skb((struct sk_buff *)pOsPkt);

    //Return the VOS packet resources.
    VosStatus = vos_pkt_return_packet( vosDataBuff );

    if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
    {
        VOS_ASSERT(0);
    }

    // JEZ110330: Now signal the layer above me...that I have released some packets.
    pctx = (BslPhyLinkCtxType *)pHddHdl;
    ppctx = pctx->pClientCtx;
    num_packets = (num_packets + 1) % 4;
    if (num_packets == 0 )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO, "%s: Sending up number of completed packets.  num_packets = %d.", __func__, num_packets );
        WLANBAP_TxPacketMonitorHandler ( (v_PVOID_t) ppctx->bapHdl ); // our handle in BAP
    }

    return(VOS_STATUS_SUCCESS);
} // WLANBAP_TxCompCB()

/**
  @brief BslFlushTxQueues() - flush the Tx  queues

  @param pPhyCtx : [in] ptr to the phy context whose queues need to be flushed

  @return
  VOS_STATUS

*/
static VOS_STATUS BslFlushTxQueues
(
    BslPhyLinkCtxType* pPhyCtx
)
{
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    v_SINT_t i = -1;
    hdd_list_node_t* pLink;
    BslTxListNodeType *pNode;


    if(TRUE == pPhyCtx->used)
    {
        while (++i != WLANTL_MAX_AC)
        {
            //Free up any packets in the Tx queue
            spin_lock_bh(&pPhyCtx->ACLTxQueue[i].lock);
            while (true)
            {
                VosStatus = hdd_list_remove_front(&pPhyCtx->ACLTxQueue[i], &pLink );
                if(VOS_STATUS_E_EMPTY != VosStatus)
                {
                    pNode = (BslTxListNodeType *)pLink;
                    kfree_skb(pNode->skb);
                    continue;
                }
                break;
            }
            spin_unlock_bh(&pPhyCtx->ACLTxQueue[i].lock);
        }
    }
    return(VOS_STATUS_SUCCESS);
} // BslFlushTxQueues


/**
  @brief BslReleasePhyCtx() - this function will free up an association context

  @param pPhyCtx : [in] ptr to the phy context to release

  @return
  None

*/
static void BslReleasePhyCtx
(
    BslPhyLinkCtxType* pPhyCtx
)
{
    uintptr_t OldMapVal;
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslReleasePhyCtx" );

    pPhyCtx->used = FALSE;


    if (BslPhyLinkMap[pPhyCtx->PhyLinkHdl].ptr == NULL) return;


    // update the phy link handle based map so TX data is stopped from flowing through
    OldMapVal = vos_atomic_set( (uintptr_t *) (BslPhyLinkMap[pPhyCtx->PhyLinkHdl].ptr),
                                    (uintptr_t) 0 );

    // clear out the Tx Queues
    VosStatus =  BslFlushTxQueues(pPhyCtx);

    // clear out the parent ptr
    //  pPhyCtx->pClientCtx = NULL;//commented to debug exception

    // we also need to remove this assocation from the list of active
    // associations maintained in the application context
    if( pPhyCtx->pPhyLinkDescNode )
    {
        VosStatus = vos_list_remove_node( &pPhyCtx->pClientCtx->PhyLinks,
                                          &((BslPhyLinksNodeType*)pPhyCtx->pPhyLinkDescNode)->node);
        VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ) );
        //Return the PhyLink handle to the free pool
        VosStatus = vos_list_insert_front(&BslPhyLinksDescPool,&((BslPhyLinksNodeType*)pPhyCtx->pPhyLinkDescNode)->node);
        VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ) );

        pPhyCtx->pPhyLinkDescNode = NULL;
    }
    pPhyCtx->pClientCtx = NULL;//Moved here to bebug the exception

    pPhyCtx->used = FALSE;

} // BslReleasePhyCtx()

/**
  @brief WLAN_BAPEventCB() - Implements the callback for ALL asynchronous events.

  Including Events resulting from:
     * HCI Create Physical Link,
     * Disconnect Physical Link,
     * Create Logical Link,
     * Flow Spec Modify,
     * HCI Reset,
     * HCI Flush,...

  Also used to return sync events locally by BSL

  @param pHddHdl: [in] The HDD(BSL) specific context for this association.
  BSL gets this from the downgoing packets Physical handle value.
  @param pBapHCIEvent: [in] pointer to the union of "HCI Event" structures.
  Contains all info needed for HCI event.
  @param AssocSpecificEvent: [in] flag indicates assoc-specific (1) or
  global (0) event

  @return
  The result code associated with performing the operation

  VOS_STATUS_E_FAULT:  pointer to pBapHCIEvent is NULL
  VOS_STATUS_SUCCESS:  Success
*/
static VOS_STATUS WLANBAP_EventCB
(
    v_PVOID_t      pHddHdl,   /* this could refer to either the BSL per
                                association context which got passed in during
                                register data plane OR the BSL per application
                                context passed in during register BAP callbacks
                                based on setting of the Boolean flag below */
    tpBtampHCI_Event pBapHCIEvent, /* This now encodes ALL event types including
                                     Command Complete and Command Status*/
    v_BOOL_t AssocSpecificEvent /* Flag to indicate global or assoc-specific event */
)
{
    BslClientCtxType* pctx;
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    vos_pkt_t* pVosPkt;
    v_U32_t PackStatus;
    static v_U8_t Buff[BSL_MAX_EVENT_SIZE]; // stack overflow?
    v_U32_t Written = 0; // FramesC REQUIRES this
    v_U32_t OldMapVal;
    struct sk_buff *skb = NULL;

    // sanity checking
    if ( pBapHCIEvent == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_EventCB bad input" );
        return VOS_STATUS_E_FAILURE;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB event=%d "
       "assoc_specific=%d", pBapHCIEvent->bapHCIEventCode, AssocSpecificEvent );

    if ( pHddHdl == NULL )
    {
        /* Consider the following error scenarios to bypass the NULL check: 
        - create LL without a call for create PL before 
        - delete LL or PL when no AMP connection has been established yet 
        Client context is unimportant from HCI point of view, only needed by the TLV API in BAP 
        TODO: Change the TLV APIs to not to carry the client context; it doesn't use it anyway 
        */
        if (( AssocSpecificEvent ) && 
            (BTAMP_TLV_HCI_PHYSICAL_LINK_COMPLETE_EVENT != pBapHCIEvent->bapHCIEventCode) &&
            (BTAMP_TLV_HCI_DISCONNECT_PHYSICAL_LINK_COMPLETE_EVENT != pBapHCIEvent->bapHCIEventCode))
        {
            pctx = gpBslctx;
        }
        else
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_FATAL, "WLANBAP_EventCB bad input" );
            return VOS_STATUS_E_FAILURE;
        }
    }


    if(NULL != pHddHdl)
    {
        if ( AssocSpecificEvent )
        {
            // get the app context from the assoc context
            pctx = ((BslPhyLinkCtxType *)pHddHdl)->pClientCtx;
        }
        else
        {
            pctx = (BslClientCtxType *)pHddHdl;
        }
    }

    if(NULL == pctx)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "pctx is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;

    }

    VosStatus = vos_pkt_get_packet( &pVosPkt, VOS_PKT_TYPE_RX_RAW,
                                    BSL_MAX_EVENT_SIZE, 1, 0, NULL, NULL);

    if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_EventCB vos_pkt_get_packet "
          "failed status=%d", VosStatus );
        return(VosStatus);
    }

    switch ( pBapHCIEvent->bapHCIEventCode )
    {
        /** BT events */
    case BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT:
    {
        /*
            BTAMP_TLV_HCI_RESET_CMD:
            BTAMP_TLV_HCI_FLUSH_CMD:
            BTAMP_TLV_HCI_LOGICAL_LINK_CANCEL_CMD:
            BTAMP_TLV_HCI_SET_EVENT_MASK_CMD:
            BTAMP_TLV_HCI_READ_CONNECTION_ACCEPT_TIMEOUT_CMD:
            BTAMP_TLV_HCI_WRITE_CONNECTION_ACCEPT_TIMEOUT_CMD:
            BTAMP_TLV_HCI_READ_LINK_SUPERVISION_TIMEOUT_CMD:
            BTAMP_TLV_HCI_WRITE_LINK_SUPERVISION_TIMEOUT_CMD:
            BTAMP_TLV_HCI_READ_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD:
            BTAMP_TLV_HCI_WRITE_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD:
            BTAMP_TLV_HCI_SET_EVENT_MASK_PAGE_2_CMD:
            BTAMP_TLV_HCI_READ_LOCATION_DATA_CMD:
            BTAMP_TLV_HCI_WRITE_LOCATION_DATA_CMD:
            BTAMP_TLV_HCI_READ_FLOW_CONTROL_MODE_CMD:
            BTAMP_TLV_HCI_WRITE_FLOW_CONTROL_MODE_CMD:
            BTAMP_TLV_HCI_READ_BEST_EFFORT_FLUSH_TO_CMD:
            BTAMP_TLV_HCI_WRITE_BEST_EFFORT_FLUSH_TO_CMD:
            BTAMP_TLV_HCI_SET_SHORT_RANGE_MODE_CMD:
            BTAMP_TLV_HCI_READ_LOCAL_VERSION_INFORMATION_CMD:
            BTAMP_TLV_HCI_READ_LOCAL_SUPPORTED_COMMANDS_CMD:
            BTAMP_TLV_HCI_READ_BUFFER_SIZE_CMD:
            BTAMP_TLV_HCI_READ_DATA_BLOCK_SIZE_CMD:
            BTAMP_TLV_HCI_READ_FAILED_CONTACT_COUNTER_CMD:
            BTAMP_TLV_HCI_RESET_FAILED_CONTACT_COUNTER_CMD:
            BTAMP_TLV_HCI_READ_LINK_QUALITY_CMD:
            BTAMP_TLV_HCI_READ_RSSI_CMD:
            BTAMP_TLV_HCI_READ_LOCAL_AMP_INFORMATION_CMD:
            BTAMP_TLV_HCI_READ_LOCAL_AMP_ASSOC_CMD:
            BTAMP_TLV_HCI_WRITE_REMOTE_AMP_ASSOC_CMD:
            BTAMP_TLV_HCI_READ_LOOPBACK_MODE_CMD:
            BTAMP_TLV_HCI_WRITE_LOOPBACK_MODE_CMD:
            BTAMP_TLV_HCI_VENDOR_SPECIFIC_CMD_0:

         */

        // pack
        PackStatus = btampPackTlvHCI_Command_Complete_Event( pctx,
                     &pBapHCIEvent->u.btampCommandCompleteEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampPackTlvHCI_Command_Complete_Event failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_COMMAND_STATUS_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Command_Status_Event( pctx,
                     &pBapHCIEvent->u.btampCommandStatusEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampPackTlvHCI_Command_Status_Event failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_NUM_OF_COMPLETED_PKTS_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Num_Completed_Pkts_Event( pctx,
                     &pBapHCIEvent->u.btampNumOfCompletedPktsEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampPackTlvHCI_Num_Completed_Pkts_Event failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_NUM_OF_COMPLETED_DATA_BLOCKS_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Num_Completed_Data_Blocks_Event( pctx,
                     &pBapHCIEvent->u.btampNumOfCompletedDataBlocksEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampPackTlvHCI_Num_Completed_Data_Blocks_Event failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_HARDWARE_ERROR_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Hardware_Error_Event( pctx,
                     &pBapHCIEvent->u.btampHardwareErrorEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_FLUSH_OCCURRED_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Flush_Occurred_Event( pctx,
                     &pBapHCIEvent->u.btampFlushOccurredEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampPackTlvHCI_Flush_Occurred_Event failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_ENHANCED_FLUSH_COMPLETE_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Enhanced_Flush_Complete_Event( pctx,
                     &pBapHCIEvent->u.btampEnhancedFlushCompleteEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampPackTlvHCI_Enhanced_Flush_Complete_Event failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_LOOPBACK_COMMAND_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Loopback_Command_Event( pctx,
                     &pBapHCIEvent->u.btampLoopbackCommandEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_DATA_BUFFER_OVERFLOW_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Data_Buffer_Overflow_Event( pctx,
                     &pBapHCIEvent->u.btampDataBufferOverflowEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_QOS_VIOLATION_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Qos_Violation_Event( pctx,
                     &pBapHCIEvent->u.btampQosViolationEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    /** BT v3.0 events */
    case BTAMP_TLV_HCI_GENERIC_AMP_LINK_KEY_NOTIFICATION_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Generic_AMP_Link_Key_Notification_Event( pctx,
                     &pBapHCIEvent->u.btampGenericAMPLinkKeyNotificationEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_PHYSICAL_LINK_COMPLETE_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Physical_Link_Complete_Event( pctx,
                     &pBapHCIEvent->u.btampPhysicalLinkCompleteEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        // look at this event to determine whether to cleanup the PHY context
        if ( pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.status ==
                WLANBAP_STATUS_SUCCESS )
        {
            // register the data plane now
            VosStatus = WLANBAP_RegisterDataPlane( pctx->bapHdl,
                                                   WLANBAP_STAFetchPktCB,
                                                   WLANBAP_STARxCB,
                                                   WLANBAP_TxCompCB,
                                                   (BslPhyLinkCtxType *)pHddHdl );

            if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
            {
                VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_EventCB WLANBAP_RegisterDataPlane "
                  "failed status = %d", VosStatus );
                // we still want to send the event upto app so do not bail
            }
            else
            {
                // update the phy link handle based map so TX data can start flowing through
                OldMapVal = vos_atomic_set( (uintptr_t*)BslPhyLinkMap+pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.phy_link_handle,
                                                (uintptr_t) pHddHdl );

//                  VOS_ASSERT( OldMapVal == 0 );//Commented to test reconnect
            }
        }
        else if ( pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.status ==
                  WLANBAP_ERROR_HOST_REJ_RESOURCES )
        {
            BslReleasePhyCtx( (BslPhyLinkCtxType *)pHddHdl );
        }
        else if ( pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.status ==
                  WLANBAP_ERROR_HOST_TIMEOUT )
        {
            //We need to update the phy link handle here to be able to reissue physical link accept
            // update the phy link handle based map so TX data can start flowing through
            OldMapVal = vos_atomic_set( (uintptr_t*)BslPhyLinkMap+pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.phy_link_handle,
                                            (uintptr_t) pHddHdl );

//                  VOS_ASSERT( OldMapVal == 0 );//Commented to test reconnect

            BslReleasePhyCtx( (BslPhyLinkCtxType *)pHddHdl );
        }
        else if ( pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.status ==
                  WLANBAP_ERROR_MAX_NUM_CNCTS )
        {
            //We need to update the phy link handle here to be able to reissue physical link /create/accept
            // update the phy link handle based map so TX data can start flowing through
            OldMapVal = vos_atomic_set( (uintptr_t*)BslPhyLinkMap+pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.phy_link_handle,
                                            (uintptr_t) pHddHdl );
//                  VOS_ASSERT( OldMapVal == 0 );//Commented to test reconnect

            BslReleasePhyCtx( (BslPhyLinkCtxType *)pHddHdl );
        }
        else if ( pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.status ==
                  WLANBAP_ERROR_HOST_TIMEOUT )
        {
            BslReleasePhyCtx( (BslPhyLinkCtxType *)pHddHdl );
        }
        else if ( pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.status ==
                  0x16 /* WLANBAP_ERROR_FAILED_CONNECTION? */ )
        {
            BslReleasePhyCtx( (BslPhyLinkCtxType *)pHddHdl );
        }
        else if ( pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.status ==
                  0x8 /* WLANBAP_ERROR_AUTH_FAILED? */ )
        {
            BslReleasePhyCtx( (BslPhyLinkCtxType *)pHddHdl );
        }
        else if ( pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.status ==
                  WLANBAP_ERROR_NO_CNCT )
        {
            BslReleasePhyCtx( (BslPhyLinkCtxType *)pHddHdl );
        }
        else
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_EventCB unexpected HCI Phy Link Comp Evt "
               "status =%d", pBapHCIEvent->u.btampPhysicalLinkCompleteEvent.status );
        }

        break;
    }
    case BTAMP_TLV_HCI_CHANNEL_SELECTED_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Channel_Selected_Event( pctx,
                     &pBapHCIEvent->u.btampChannelSelectedEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_DISCONNECT_PHYSICAL_LINK_COMPLETE_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Disconnect_Physical_Link_Complete_Event( pctx,
                     &pBapHCIEvent->u.btampDisconnectPhysicalLinkCompleteEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        // we need to cleanup the PHY context always but have these checks to make
        // sure we catch unexpected behavior, strangely enough even when peer triggers
        // the disconnect the reason code is still 0x16, weird
        if ( pBapHCIEvent->u.btampDisconnectPhysicalLinkCompleteEvent.status == WLANBAP_STATUS_SUCCESS &&
                pBapHCIEvent->u.btampDisconnectPhysicalLinkCompleteEvent.reason == WLANBAP_ERROR_TERM_BY_LOCAL_HOST )
        {
            BslReleasePhyCtx( (BslPhyLinkCtxType *)pHddHdl );
        }
        else
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_EventCB unexpected HCI Dis Phy Link Comp Evt "
               "status =%d reason =%d", pBapHCIEvent->u.btampDisconnectPhysicalLinkCompleteEvent.status,
                       pBapHCIEvent->u.btampDisconnectPhysicalLinkCompleteEvent.reason );
        }

        break;
    }
    case BTAMP_TLV_HCI_PHYSICAL_LINK_LOSS_WARNING_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Physical_Link_Loss_Warning_Event( pctx,
                     &pBapHCIEvent->u.btampPhysicalLinkLossWarningEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_PHYSICAL_LINK_RECOVERY_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Physical_Link_Recovery_Event( pctx,
                     &pBapHCIEvent->u.btampPhysicalLinkRecoveryEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_LOGICAL_LINK_COMPLETE_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Logical_Link_Complete_Event( pctx,
                     &pBapHCIEvent->u.btampLogicalLinkCompleteEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_DISCONNECT_LOGICAL_LINK_COMPLETE_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Disconnect_Logical_Link_Complete_Event( pctx,
                     &pBapHCIEvent->u.btampDisconnectLogicalLinkCompleteEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_FLOW_SPEC_MODIFY_COMPLETE_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Flow_Spec_Modify_Complete_Event( pctx,
                     &pBapHCIEvent->u.btampFlowSpecModifyCompleteEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    case BTAMP_TLV_HCI_SHORT_RANGE_MODE_CHANGE_COMPLETE_EVENT:
    {
        // pack
        PackStatus = btampPackTlvHCI_Short_Range_Mode_Change_Complete_Event( pctx,
                     &pBapHCIEvent->u.btampShortRangeModeChangeCompleteEvent, Buff, BSL_MAX_EVENT_SIZE, &Written );

        if ( !BTAMP_SUCCEEDED( PackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_EventCB: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", PackStatus);
            // handle the error
            VosStatus = vos_pkt_return_packet( pVosPkt );

            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

        break;
    }
    default:
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_EventCB unexpected event" );

        VosStatus = vos_pkt_return_packet( pVosPkt );

        VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

        return(VOS_STATUS_E_FAILURE);
        break;
    }
    }

    VOS_ASSERT(Written <= BSL_MAX_EVENT_SIZE);

    // stick the event into a VoS pkt
    VosStatus = vos_pkt_push_head( pVosPkt, Buff, Written );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_EventCB vos_pkt_push_head "
          "status =%d", VosStatus );

            // return the packet
            VosStatus = vos_pkt_return_packet( pVosPkt );
            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

            return(VOS_STATUS_E_FAILURE);
        }

    // Extract the OS packet (skb).
    // Tell VOS to detach the OS packet from the VOS packet
    VosStatus = vos_pkt_get_os_packet( pVosPkt, (v_VOID_t **)&skb, VOS_TRUE );
    if(!VOS_IS_STATUS_SUCCESS( VosStatus ))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "%s: Failure extracting skb from vos pkt. "
          "VosStatus = %d", __func__, VosStatus );

        // return the packet
        VosStatus = vos_pkt_return_packet( pVosPkt );
        VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

        return(VOS_STATUS_E_FAILURE);
    }

    //JEZ100922: We are free to return the enclosing VOSS packet.
    VosStatus = vos_pkt_return_packet( pVosPkt );
    VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ));

    //JEZ100809: While an skb is being handled by the kernel, is "skb->dev" de-ref'd?
    skb->dev = (struct net_device *) gpBslctx->hdev;
    bt_cb(skb)->pkt_type = HCI_EVENT_PKT;
    //skb->protocol = eth_type_trans(skb, skb->dev);
    //skb->ip_summed = CHECKSUM_UNNECESSARY;

    // This is my receive skb pointer
    gpBslctx->rx_skb = skb;

    // This is how data and events are passed up to BlueZ
    hci_recv_frame(gpBslctx->rx_skb);

    return(VOS_STATUS_SUCCESS);
} // WLANBAP_EventCB()

static VOS_STATUS  
WLANBAP_PhyLinkFailure
( 
    BslClientCtxType* pctx,
    v_U8_t       phy_link_handle
)
{
    VOS_STATUS  vosStatus;
    tBtampHCI_Event bapHCIEvent;

    /* Format the Physical Link Complete event to return... */ 
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_PHYSICAL_LINK_COMPLETE_EVENT;
    bapHCIEvent.u.btampPhysicalLinkCompleteEvent.present = 1;
    bapHCIEvent.u.btampPhysicalLinkCompleteEvent.status = WLANBAP_ERROR_UNSPECIFIED_ERROR;
    bapHCIEvent.u.btampPhysicalLinkCompleteEvent.phy_link_handle 
        = phy_link_handle;
    bapHCIEvent.u.btampPhysicalLinkCompleteEvent.ch_number 
        = 0;
    //TBD: Could be a cleaner way to get the PhyLinkCtx handle; For now works
    BslPhyLinkCtx[0].pClientCtx = pctx;
    vosStatus = WLANBAP_EventCB( &BslPhyLinkCtx[0], &bapHCIEvent, TRUE );

    return vosStatus;
}

/**
  @brief BslFindAndInitClientCtx() - This function will find and initialize a client
  a.k.a app context

  @param pctx : [inout] ptr to the client context

  @return
  TRUE if all OK, FALSE otherwise

*/
static BOOL BslFindAndInitClientCtx
(
    BslClientCtxType** pctx_
)
{
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    BslClientCtxType* pctx;
    v_U8_t i;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslFindAndInitClientCtx" );

    if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
    {
        VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,"%s:BslClientLock already inited",__func__);
        // return(0);
    }

    for ( i=0; i < BSL_MAX_CLIENTS; i++ )
    {
        if ( !BslClientCtx[i].used )
        {
            VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,"%s:BslClientCtx[%d] selected",__func__, i);
            BslClientCtx[i].used = TRUE;
            break;
        }
    }

    if ( i == BSL_MAX_CLIENTS )
    {
        // no more clients can be supported
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BslFindAndInitClientCtx no more "
          "clients can be supported MAX=%d", BSL_MAX_CLIENTS );
        return FALSE;
    }

    //pctx = BslClientCtx + i;
    pctx = gpBslctx;

    // get a handle from BAP
    VosStatus = WLANBAP_GetNewHndl(&pctx->bapHdl);

    if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
    {
        pctx->used = FALSE;

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s:WLAN_GetNewHndl Failed",__func__);

        return(FALSE);
    }

    // register the event cb with BAP, this cb is used for BOTH association
    // specific and non-association specific event notifications by BAP.
    // However association specific events will be called with a different
    // cookie that is passed in during the physical link create/accept
    VosStatus = WLAN_BAPRegisterBAPCallbacks( pctx->bapHdl, WLANBAP_EventCB, pctx );

    if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
    {
        pctx->used = FALSE;

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s:WLAN_BAPRegsiterBAPCallaback Failed",__func__);

        return(FALSE);
    }

    // init the PhyLinks queue to keep track of the assoc's of this client
    VosStatus = vos_list_init( &pctx->PhyLinks );
    VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ) );

    *pctx_ = pctx;

    return(TRUE);
} //BslFindAndInitClientCtx()

/**
  @brief BslReleaseClientCtx() - This function will release a client a.k.a. app
  context

  @param pctx : [in] ptr to the client context

  @return
  None

*/
//#if 0
static void BslReleaseClientCtx
(
    BslClientCtxType* pctx
)
{
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    vos_list_node_t* pLink;
    BslPhyLinksNodeType *pPhyNode;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_LOW, "BslReleaseClientCtx" );

    // an app can do this without cleaning up after itself i.e. it can have active associations and
    // data pending, we need to cleanup its mess

    // first tell BAP we dont want the handle anymore, BAP will cleanup all the associations and
    // consume resulting HCI events, so after this we will not get any HCI events. we will also
    // not see any FetchPktCB and RxPktCB. We can still expect TxCompletePktCB
    VosStatus = WLANBAP_ReleaseHndl( pctx->bapHdl );
    VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ) );


    // find and free all of the association contexts belonging to this app
    while ( VOS_IS_STATUS_SUCCESS( VosStatus = vos_list_remove_front( &pctx->PhyLinks, &pLink ) ) )
    {
        pPhyNode = (BslPhyLinksNodeType *)pLink;

        // since the phy link has already been removed from the list of active
        // associations, make sure we dont attempt to do this again
        pPhyNode->pPhy->pPhyLinkDescNode = NULL;

        BslReleasePhyCtx( pPhyNode->pPhy );
    }

    VOS_ASSERT( VosStatus == VOS_STATUS_E_EMPTY );

    // destroy the PhyLinks queue
    VosStatus = vos_list_destroy( &pctx->PhyLinks );
    VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ) );

    pctx->used = FALSE;

} // BslReleaseClientCtx()
//#endif

/**
  @brief BslInitPhyCtx() - Initialize the Phy Context array.


  @return
  TRUE if all OK, FALSE otherwise

*/
static BOOL BslInitPhyCtx (void)
{
    v_U16_t i;
    // free PHY context

    for ( i=0; i<BSL_MAX_PHY_LINKS; i++ )
    {
        BslPhyLinkCtx[i].used = FALSE;
    }

    return (TRUE);
} // BslInitPhyCtx()


/**
  @brief BslFindAndInitPhyCtx() - This function will try to find a free physical
  link a.k.a assocation context and if successful, then init that context

  @param pctx : [in] the client context
  @param PhyLinkHdl : [in] the physical link handle chosen by application
  @param ppPhyCtx : [inout] ptr to the physical link context

  @return
  TRUE if all OK, FALSE otherwise

*/
static BOOL BslFindAndInitPhyCtx
(
    BslClientCtxType*   pctx,
    v_U8_t              PhyLinkHdl,
    BslPhyLinkCtxType** ppPhyCtx
)
{
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    v_U16_t i;
    v_U16_t j;
    vos_list_node_t* pLink;
    BslPhyLinksNodeType *pNode;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_LOW, "BslFindAndInitPhyCtx" );

    for ( i=0; i<BSL_MAX_PHY_LINKS; i++ )
    {
        if ( !BslPhyLinkCtx[i].used )
        {
            BslPhyLinkCtx[i].used = TRUE;
            break;
        }
    }

    if ( i==BSL_MAX_PHY_LINKS )
    {
        return(FALSE);
    }
    else
    {

        // now init this context

        *ppPhyCtx = BslPhyLinkCtx + i;

        // setup a ptr to the app context that this assocation specific context lives in
        BslPhyLinkCtx[i].pClientCtx = pctx;

        // Mark as used
        (*ppPhyCtx)->used = TRUE;

        // store the PHY link handle
        BslPhyLinkCtx[i].PhyLinkHdl = PhyLinkHdl;

        // init the TX queues
        for ( j=0; j<WLANTL_MAX_AC; j++ )
        {
            hdd_list_init( &BslPhyLinkCtx[i].ACLTxQueue[j], HDD_TX_QUEUE_MAX_LEN );
            //VosStatus = vos_list_init( &BslPhyLinkCtx[i].ACLTxQueue[j] );
            //VosStatus = vos_list_init( &(BslPhyLinkCtx+i)->ACLTxQueue );
            VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ) );
        }

        // need to add this Phy context to the client list of associations,
        // useful during Close operation

        // get a pkt desc
        VosStatus = vos_list_remove_front( &BslPhyLinksDescPool, &pLink );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            // this could happen due to pool not being big enough, etc
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_LOW, "BslFindAndInitPhyCtx failed to "
             "get node from BslPhyLinksDescPool vstatus=%d", VosStatus );
            BslReleasePhyCtx( *ppPhyCtx );
            return FALSE;
        }

        // stick the VOS pkt into the node
        pNode = (BslPhyLinksNodeType *) pLink;
        pNode->node = *pLink;
        pNode->pPhy = *ppPhyCtx;


        // now queue the pkt into the correct queue
        VosStatus = vos_list_insert_back( &pctx->PhyLinks, pLink );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_ASSERT(0);
        }

        // need to record the desc for this assocation in the list of
        // active assocations in client context to allow cleanup later
        (*ppPhyCtx)->pPhyLinkDescNode = pNode;

        return(TRUE);
    }
} // BslFindAndInitPhyCtx()

/**
  @brief BslProcessHCICommand() - This function will process an HCI command i.e
  take an HCI command buffer, unpack it and then call the appropriate BAP API

  @param pctx : [in] ptr to the client context
  @param pBuffer_ : [in] the input buffer containing the HCI command
  @param Count_ : [in] size of the HCI command buffer

  @return
  TRUE if all OK, FALSE otherwise

*/
static BOOL BslProcessHCICommand
(
    BslClientCtxType* pctx,
    LPCVOID pBuffer_,
    DWORD Count_
)
{
    LPVOID pBuffer = (LPVOID) pBuffer_; // castaway the const-ness of the ptr
    v_U16_t Count = (v_U16_t) Count_;  // this should be OK max size < 1500
    v_U32_t UnpackStatus;
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    BOOL Status;
    BslPhyLinkCtxType* pPhyCtx;
    tBtampHCI_Event HCIEvt;
    v_U16_t x = 1;
    int i = 0;

    // the opcode is in LE, if we are LE too then this is fine else we need some
    // byte swapping
    v_U16_t cmdOpcode = *(UNALIGNED v_U16_t *)pBuffer;
    v_U8_t *pBuf = (v_U8_t *)pBuffer;
    v_U8_t *pTmp = (v_U8_t *)pBuf;

    // TODO: do we really need to do this per call even though the op is quite cheap
    if(*(v_U8_t *)&x == 0)
    {
        // BE
        cmdOpcode = ( cmdOpcode & 0xFF ) << 8 | ( cmdOpcode & 0xFF00 ) >> 8;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_LOW, "BslProcessHCICommand: cmdOpcode = %hx", cmdOpcode );

    for(i=0; i<4; i++)
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: *pBuf before advancepTmp[%x] = %x", i,pTmp[i] );

    pBuf+=CMD_TLV_TYPE_AND_LEN_SIZE;


    switch ( cmdOpcode )
    {
        /** BT v3.0 Link Control commands */
    case BTAMP_TLV_HCI_CREATE_PHYSICAL_LINK_CMD:
    {
        tBtampTLVHCI_Create_Physical_Link_Cmd CreatePhysicalLinkCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Create_Physical_Link_Cmd( NULL,
                       pBuf, Count, &CreatePhysicalLinkCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Create_Physical_Link_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        if(VOS_FALSE == WLANBAP_AmpConnectionAllowed())
        {
            VosStatus = WLANBAP_PhyLinkFailure(pctx, CreatePhysicalLinkCmd.phy_link_handle);
            if ( VOS_STATUS_SUCCESS != VosStatus )
            {
                VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BslProcessHCICommand: WLANBAP_PhyLinkFailure failed");
                // handle the error
                return(FALSE);
            }
            break;
        }

        // setup the per PHY link BAP context
        Status = BslFindAndInitPhyCtx( pctx, CreatePhysicalLinkCmd.phy_link_handle,
                                       &pPhyCtx );

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "CreatePhysicalLinkCmd.phy_link_handle=%d",CreatePhysicalLinkCmd.phy_link_handle);

        if ( !Status )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: BslFindAndInitPhyCtx failed");
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPPhysicalLinkCreate( pctx->bapHdl,
                                                &CreatePhysicalLinkCmd, pPhyCtx, &HCIEvt );


        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPPhysicalLinkCreate failed status %d", VosStatus);
            // handle the error
            BslReleasePhyCtx( pPhyCtx );
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pPhyCtx, &HCIEvt, TRUE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            BslReleasePhyCtx( pPhyCtx );
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_ACCEPT_PHYSICAL_LINK_CMD:
    {
        tBtampTLVHCI_Accept_Physical_Link_Cmd AcceptPhysicalLinkCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Accept_Physical_Link_Cmd( NULL,
                       pBuf, Count, &AcceptPhysicalLinkCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Accept_Physical_Link_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        if(VOS_FALSE == WLANBAP_AmpConnectionAllowed())
        {
            VosStatus = WLANBAP_PhyLinkFailure(pctx, AcceptPhysicalLinkCmd.phy_link_handle);
            if ( VOS_STATUS_SUCCESS != VosStatus )
            {
                VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BslProcessHCICommand: WLANBAP_PhyLinkFailure failed");
                // handle the error
                return(FALSE);
            }
            break;
        }

        // setup the per PHY link BAP context
        Status = BslFindAndInitPhyCtx( pctx, AcceptPhysicalLinkCmd.phy_link_handle,
                                       &pPhyCtx );

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "AcceptPhysicalLinkCmd.phy_link_handle=%d",AcceptPhysicalLinkCmd.phy_link_handle);

        if ( !Status )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: BslFindAndInitPhyCtx failed");
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPPhysicalLinkAccept( pctx->bapHdl,
                                                &AcceptPhysicalLinkCmd, pPhyCtx, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPPhysicalLinkAccept failed status %d", VosStatus);
            // handle the error
            BslReleasePhyCtx( pPhyCtx );
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pPhyCtx, &HCIEvt, TRUE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            BslReleasePhyCtx( pPhyCtx );
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_DISCONNECT_PHYSICAL_LINK_CMD:
    {
        tBtampTLVHCI_Disconnect_Physical_Link_Cmd DisconnectPhysicalLinkCmd;
        Count = Count - 3;//Type and length field lengths are not needed
        pTmp = pBuf;
        for(i=0; i<4; i++)
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: *pBuf in Disconnect phy link pTmp[%x] = %x", i,pTmp[i] );
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Disconnect_Physical_Link_Cmd( NULL,
                       pBuf, Count, &DisconnectPhysicalLinkCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Disconnect_Physical_Link_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPPhysicalLinkDisconnect( pctx->bapHdl,
                    &DisconnectPhysicalLinkCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPPhysicalLinkDisconnect failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_CREATE_LOGICAL_LINK_CMD:
    {
        tBtampTLVHCI_Create_Logical_Link_Cmd CreateLogicalLinkCmd;
        Count -= 3; //To send the correct length to unpack event
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Create_Logical_Link_Cmd( NULL,
                       pBuf, Count, &CreateLogicalLinkCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Create_Logical_Link_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPLogicalLinkCreate( pctx->bapHdl,
                                               &CreateLogicalLinkCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPLogicalLinkCreate failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_ACCEPT_LOGICAL_LINK_CMD:
    {
        tBtampTLVHCI_Accept_Logical_Link_Cmd AcceptLogicalLinkCmd;
        Count = Count - 3;//Subtract Type and Length fields
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Accept_Logical_Link_Cmd( NULL,
                       pBuf, Count, &AcceptLogicalLinkCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Accept_Logical_Link_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPLogicalLinkAccept( pctx->bapHdl,
                                               &AcceptLogicalLinkCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPLogicalLinkAccept failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_DISCONNECT_LOGICAL_LINK_CMD:
    {
        tBtampTLVHCI_Disconnect_Logical_Link_Cmd DisconnectLogicalLinkCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Disconnect_Logical_Link_Cmd( NULL,
                       pBuf, Count, &DisconnectLogicalLinkCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Disconnect_Logical_Link_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPLogicalLinkDisconnect( pctx->bapHdl,
                    &DisconnectLogicalLinkCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPLogicalLinkDisconnect failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_LOGICAL_LINK_CANCEL_CMD:
    {
        tBtampTLVHCI_Logical_Link_Cancel_Cmd LogicalLinkCancelCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Logical_Link_Cancel_Cmd( NULL,
                       pBuf, Count, &LogicalLinkCancelCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Logical_Link_Cancel_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPLogicalLinkCancel( pctx->bapHdl,
                                               &LogicalLinkCancelCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPLogicalLinkCancel failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_FLOW_SPEC_MODIFY_CMD:
    {
        tBtampTLVHCI_Flow_Spec_Modify_Cmd FlowSpecModifyCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Flow_Spec_Modify_Cmd( NULL,
                       pBuf, Count, &FlowSpecModifyCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Flow_Spec_Modify_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPFlowSpecModify( pctx->bapHdl,
                                            &FlowSpecModifyCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPFlowSpecModify failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    /*
      Host Controller and Baseband Commands
    */
    case BTAMP_TLV_HCI_RESET_CMD:
    {
        VosStatus = WLAN_BAPReset( pctx->bapHdl );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReset failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_SET_EVENT_MASK_CMD:
    {
        tBtampTLVHCI_Set_Event_Mask_Cmd SetEventMaskCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Set_Event_Mask_Cmd( NULL,
                       pBuf, Count, &SetEventMaskCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Set_Event_Mask_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPSetEventMask( pctx->bapHdl,
                                          &SetEventMaskCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPSetEventMask failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_FLUSH_CMD:
    {
        tBtampTLVHCI_Flush_Cmd FlushCmd;

        // unpack
        UnpackStatus = btampUnpackTlvHCI_Flush_Cmd( NULL,
                       pBuf, Count, &FlushCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Flush_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        /* Flush the TX queue */
//#ifdef BAP_DEBUG
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "%s:HCI Flush command  - will flush Tx Queue", __func__);
//#endif //BAP_DEBUG
        // JEZ100604: Temporary short cut
        pPhyCtx = &BslPhyLinkCtx[0];
        VosStatus = BslFlushTxQueues ( pPhyCtx);

        /* Acknowledge the command */
        VosStatus = WLAN_BAPFlush( pctx->bapHdl, &FlushCmd );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BslProcessHCICommand: WLAN_BAPFlush failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_ENHANCED_FLUSH_CMD:
    {
        tBtampTLVHCI_Enhanced_Flush_Cmd FlushCmd;

        // unpack
        UnpackStatus = btampUnpackTlvHCI_Enhanced_Flush_Cmd( NULL,
                                                             pBuf, Count, &FlushCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BslProcessHCICommand: btampUnpackTlvHCI_Enhanced_Flush_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        /* Flush the TX queue */
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s:HCI Flush command  - will flush Tx Queue for pkt type %d", __func__, FlushCmd.packet_type);
        // We support BE traffic only
        if(WLANTL_AC_BE == FlushCmd.packet_type)
        {
            pPhyCtx = &BslPhyLinkCtx[0];
            VosStatus = BslFlushTxQueues ( pPhyCtx);
        }

        /* Acknowledge the command */
        VosStatus = WLAN_EnhancedBAPFlush( pctx->bapHdl, &FlushCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BslProcessHCICommand: WLAN_EnahncedBAPFlush failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_CONNECTION_ACCEPT_TIMEOUT_CMD:
    {
        VosStatus = WLAN_BAPReadConnectionAcceptTimeout( pctx->bapHdl, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadConnectionAcceptTimeout failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_WRITE_CONNECTION_ACCEPT_TIMEOUT_CMD:
    {
        tBtampTLVHCI_Write_Connection_Accept_Timeout_Cmd WriteConnectionAcceptTimeoutCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Write_Connection_Accept_Timeout_Cmd( NULL,
                       pBuf, Count, &WriteConnectionAcceptTimeoutCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Write_Connection_Accept_Timeout_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPWriteConnectionAcceptTimeout( pctx->bapHdl,
                    &WriteConnectionAcceptTimeoutCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPWriteConnectionAcceptTimeout failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_LINK_SUPERVISION_TIMEOUT_CMD:
    {
        tBtampTLVHCI_Read_Link_Supervision_Timeout_Cmd ReadLinkSupervisionTimeoutCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Read_Link_Supervision_Timeout_Cmd( NULL,
                       pBuf, Count, &ReadLinkSupervisionTimeoutCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Read_Link_Supervision_Timeout_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPReadLinkSupervisionTimeout( pctx->bapHdl,
                    &ReadLinkSupervisionTimeoutCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadLinkSupervisionTimeout failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_WRITE_LINK_SUPERVISION_TIMEOUT_CMD:
    {
        tBtampTLVHCI_Write_Link_Supervision_Timeout_Cmd WriteLinkSupervisionTimeoutCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Write_Link_Supervision_Timeout_Cmd( NULL,
                       pBuf, Count, &WriteLinkSupervisionTimeoutCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Write_Link_Supervision_Timeout_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPWriteLinkSupervisionTimeout( pctx->bapHdl,
                    &WriteLinkSupervisionTimeoutCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPWriteLinkSupervisionTimeout failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    /* v3.0 Host Controller and Baseband Commands */
    case BTAMP_TLV_HCI_READ_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD:
    {
        VosStatus = WLAN_BAPReadLogicalLinkAcceptTimeout( pctx->bapHdl, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadLogicalLinkAcceptTimeout failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_WRITE_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD:
    {
        tBtampTLVHCI_Write_Logical_Link_Accept_Timeout_Cmd WriteLogicalLinkAcceptTimeoutCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Write_Logical_Link_Accept_Timeout_Cmd( NULL,
                       pBuf, Count, &WriteLogicalLinkAcceptTimeoutCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Write_Logical_Link_Accept_Timeout_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPWriteLogicalLinkAcceptTimeout( pctx->bapHdl,
                    &WriteLogicalLinkAcceptTimeoutCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPWriteLogicalLinkAcceptTimeout failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_SET_EVENT_MASK_PAGE_2_CMD:
    {
        tBtampTLVHCI_Set_Event_Mask_Page_2_Cmd SetEventMaskPage2Cmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Set_Event_Mask_Page_2_Cmd( NULL,
                       pBuf, Count, &SetEventMaskPage2Cmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Set_Event_Mask_Page_2_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPSetEventMaskPage2( pctx->bapHdl,
                                               &SetEventMaskPage2Cmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPSetEventMaskPage2 failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_LOCATION_DATA_CMD:
    {
        VosStatus = WLAN_BAPReadLocationData( pctx->bapHdl, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadLocationData failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_WRITE_LOCATION_DATA_CMD:
    {
        tBtampTLVHCI_Write_Location_Data_Cmd WriteLocationDataCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Write_Location_Data_Cmd( NULL,
                       pBuf, Count, &WriteLocationDataCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Write_Location_Data_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPWriteLocationData( pctx->bapHdl,
                                               &WriteLocationDataCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPWriteLocationData failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_FLOW_CONTROL_MODE_CMD:
    {
        VosStatus = WLAN_BAPReadFlowControlMode( pctx->bapHdl, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadFlowControlMode failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_WRITE_FLOW_CONTROL_MODE_CMD:
    {
        tBtampTLVHCI_Write_Flow_Control_Mode_Cmd WriteFlowControlModeCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Write_Flow_Control_Mode_Cmd( NULL,
                       pBuf, Count, &WriteFlowControlModeCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Write_Flow_Control_Mode_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPWriteFlowControlMode( pctx->bapHdl,
                    &WriteFlowControlModeCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPWriteFlowControlMode failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_BEST_EFFORT_FLUSH_TIMEOUT_CMD:
    {
        tBtampTLVHCI_Read_Best_Effort_Flush_Timeout_Cmd ReadBestEffortFlushTimeoutCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Read_Best_Effort_Flush_Timeout_Cmd( NULL,
                       pBuf, Count, &ReadBestEffortFlushTimeoutCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Read_Best_Effort_Flush_Timeout_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPReadBestEffortFlushTimeout( pctx->bapHdl,
                    &ReadBestEffortFlushTimeoutCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadBestEffortFlushTimeout failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_WRITE_BEST_EFFORT_FLUSH_TIMEOUT_CMD:
    {
        tBtampTLVHCI_Write_Best_Effort_Flush_Timeout_Cmd WriteBestEffortFlushTimeoutCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Write_Best_Effort_Flush_Timeout_Cmd( NULL,
                       pBuf, Count, &WriteBestEffortFlushTimeoutCmd);

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Write_Best_Effort_Flush_Timeout_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPWriteBestEffortFlushTimeout( pctx->bapHdl,
                    &WriteBestEffortFlushTimeoutCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPWriteBestEffortFlushTimeout failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    /** opcode definition for this command from AMP HCI CR D9r4 markup */
    case BTAMP_TLV_HCI_SET_SHORT_RANGE_MODE_CMD:
    {
        tBtampTLVHCI_Set_Short_Range_Mode_Cmd SetShortRangeModeCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Set_Short_Range_Mode_Cmd( NULL,
                       pBuf, Count, &SetShortRangeModeCmd);

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Set_Short_Range_Mode_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPSetShortRangeMode( pctx->bapHdl,
                                               &SetShortRangeModeCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPSetShortRangeMode failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    /* End of v3.0 Host Controller and Baseband Commands */
    /*
       Informational Parameters
    */
    case BTAMP_TLV_HCI_READ_LOCAL_VERSION_INFO_CMD:
    {
        VosStatus = WLAN_BAPReadLocalVersionInfo( pctx->bapHdl, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadLocalVersionInfo failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_LOCAL_SUPPORTED_CMDS_CMD:
    {
        VosStatus = WLAN_BAPReadLocalSupportedCmds( pctx->bapHdl, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadLocalSupportedCmds failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_BUFFER_SIZE_CMD:
    {
        VosStatus = WLAN_BAPReadBufferSize( pctx->bapHdl, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadBufferSize failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    /* v3.0 Informational commands */
    case BTAMP_TLV_HCI_READ_DATA_BLOCK_SIZE_CMD:
    {
        VosStatus = WLAN_BAPReadDataBlockSize( pctx->bapHdl, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadDataBlockSize failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    /*
      Status Parameters
    */
    case BTAMP_TLV_HCI_READ_FAILED_CONTACT_COUNTER_CMD:
    {
        tBtampTLVHCI_Read_Failed_Contact_Counter_Cmd ReadFailedContactCounterCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Read_Failed_Contact_Counter_Cmd( NULL,
                       pBuf, Count, &ReadFailedContactCounterCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Read_Failed_Contact_Counter_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPReadFailedContactCounter( pctx->bapHdl,
                    &ReadFailedContactCounterCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadFailedContactCounter failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_RESET_FAILED_CONTACT_COUNTER_CMD:
    {
        tBtampTLVHCI_Reset_Failed_Contact_Counter_Cmd ResetFailedContactCounterCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Reset_Failed_Contact_Counter_Cmd( NULL,
                       pBuf, Count, &ResetFailedContactCounterCmd);

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Reset_Failed_Contact_Counter_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPResetFailedContactCounter( pctx->bapHdl,
                    &ResetFailedContactCounterCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPResetFailedContactCounter failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_LINK_QUALITY_CMD:
    {
        tBtampTLVHCI_Read_Link_Quality_Cmd ReadLinkQualityCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Read_Link_Quality_Cmd( NULL,
                       pBuf, Count, &ReadLinkQualityCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Read_Link_Quality_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPReadLinkQuality( pctx->bapHdl,
                                             &ReadLinkQualityCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadLinkQuality failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_RSSI_CMD:
    {
        tBtampTLVHCI_Read_RSSI_Cmd ReadRssiCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Read_RSSI_Cmd( NULL,
                       pBuf, Count, &ReadRssiCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Read_RSSI_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPReadRSSI( pctx->bapHdl,
                                      &ReadRssiCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadRSSI failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_LOCAL_AMP_INFORMATION_CMD:
    {
        tBtampTLVHCI_Read_Local_AMP_Information_Cmd ReadLocalAmpInformationCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Read_Local_AMP_Information_Cmd( NULL,
                       pBuf, Count, &ReadLocalAmpInformationCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Read_Local_AMP_Information_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPReadLocalAMPInfo( pctx->bapHdl,
                                              &ReadLocalAmpInformationCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadLocalAMPInfo failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_READ_LOCAL_AMP_ASSOC_CMD:
    {
        tBtampTLVHCI_Read_Local_AMP_Assoc_Cmd ReadLocalAmpAssocCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Read_Local_AMP_Assoc_Cmd( NULL,
                       pBuf, Count, &ReadLocalAmpAssocCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Read_Local_AMP_Assoc_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPReadLocalAMPAssoc( pctx->bapHdl,
                                               &ReadLocalAmpAssocCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadLocalAMPAssoc failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_WRITE_REMOTE_AMP_ASSOC_CMD:
    {
        tBtampTLVHCI_Write_Remote_AMP_ASSOC_Cmd WriteRemoteAmpAssocCmd;
        // unpack

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: HCI_Write_Remote_AMP_ASSOC_Cmd Count = %d", Count);
        DUMPLOG(1, __func__, "HCI_Write_Remote_AMP_ASSOC cmd",
                pBuf,
                Count);

        UnpackStatus = btampUnpackTlvHCI_Write_Remote_AMP_ASSOC_Cmd( NULL,
                       pBuf, Count, &WriteRemoteAmpAssocCmd );

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WriteRemoteAmpAssocCmd.amp_assoc_remaining_length = %d",
                   WriteRemoteAmpAssocCmd.amp_assoc_remaining_length
                 );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Write_Remote_AMP_ASSOC_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

//#define BAP_UNIT_TEST
#ifdef BAP_UNIT_TEST
        {
            unsigned char test_amp_assoc_fragment[] =
            {
                0x01, 0x00, 0x06, 0x00, 0x00, 0xde, 0xad, 0xbe,
                0xef, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
                0x0c, 0x00, 0x55, 0x53, 0x20, 0xc9, 0x0c, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x03, 0x00, 0x06, 0x00, 0x55, 0x53,
                0x20, 0xc9, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x04, 0x00, 0x04, 0x00, 0x03, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00,
                0x00, 0xf5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00
            };
            WriteRemoteAmpAssocCmd.present = 1;
            WriteRemoteAmpAssocCmd.phy_link_handle = 1;
            WriteRemoteAmpAssocCmd.length_so_far = 0;
            WriteRemoteAmpAssocCmd.amp_assoc_remaining_length = 74;
            /* Set the amp_assoc_fragment to the right values of MAC addr and
             * channels
             */
            vos_mem_copy(
                WriteRemoteAmpAssocCmd.amp_assoc_fragment,
                test_amp_assoc_fragment,
                sizeof( test_amp_assoc_fragment));

        }
#endif

        VosStatus = WLAN_BAPWriteRemoteAMPAssoc( pctx->bapHdl,
                    &WriteRemoteAmpAssocCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPWriteRemoteAMPAssoc failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    /*
      Debug Commands
    */
    case BTAMP_TLV_HCI_READ_LOOPBACK_MODE_CMD:
    {
        tBtampTLVHCI_Read_Loopback_Mode_Cmd ReadLoopbackModeCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Read_Loopback_Mode_Cmd( NULL,
                       pBuf, Count, &ReadLoopbackModeCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Read_Loopback_Mode_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPReadLoopbackMode( pctx->bapHdl,
                                              &ReadLoopbackModeCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPReadLoopbackMode failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_WRITE_LOOPBACK_MODE_CMD:
    {
        tBtampTLVHCI_Write_Loopback_Mode_Cmd WriteLoopbackModeCmd;
        // unpack
        UnpackStatus = btampUnpackTlvHCI_Write_Loopback_Mode_Cmd( NULL,
                       pBuf, Count, &WriteLoopbackModeCmd );

        if ( !BTAMP_SUCCEEDED( UnpackStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: btampUnpackTlvHCI_Write_Loopback_Mode_Cmd failed status %d", UnpackStatus);
            // handle the error
            return(FALSE);
        }

        VosStatus = WLAN_BAPWriteLoopbackMode( pctx->bapHdl,
                                               &WriteLoopbackModeCmd, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPWriteLoopbackMode failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_VENDOR_SPECIFIC_CMD_0:
    {
        VosStatus = WLAN_BAPVendorSpecificCmd0( pctx->bapHdl, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPVendorSpecificCmd0 failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    case BTAMP_TLV_HCI_VENDOR_SPECIFIC_CMD_1:
    {
        VosStatus = WLAN_BAPVendorSpecificCmd1( pctx->bapHdl, &HCIEvt );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLAN_BAPVendorSpecificCmd1 failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }

        break;
    }
    default:
    {
        /* Unknow opcode. Return a command status event...with "Unknown Opcode" status  */

        /* Format the command status event to return... */
        HCIEvt.bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
        HCIEvt.u.btampCommandStatusEvent.present = 1;
        HCIEvt.u.btampCommandStatusEvent.status = WLANBAP_ERROR_UNKNOWN_HCI_CMND;
        HCIEvt.u.btampCommandStatusEvent.num_hci_command_packets = 1;
        HCIEvt.u.btampCommandStatusEvent.command_opcode
        = cmdOpcode;

        // this may look strange as this is the function registered
        // with BAP for the EventCB but we are also going to use it
        // as a helper function. The difference is that this invocation
        // runs in HCI command sending caller context while the callback
        // will happen in BAP's context whatever that may be
        VosStatus = WLANBAP_EventCB( pctx, &HCIEvt, FALSE );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BslProcessHCICommand: WLANBAP_EventCB failed status %d", VosStatus);
            // handle the error
            return(FALSE);
        }


        break;
    }
    }

    return(TRUE);
} // BslProcessHCICommand()


/**
  @brief BslProcessACLDataTx() - This function will process an egress ACL data packet

  @param pctx : [in] ptr to the client context
  @param pBuffer_ : [in] ptr to the buffer containing the ACL data packet
  @param pCount : [in] size of the ACL data packet buffer

  @return
  TRUE if all OK, FALSE otherwise

*/
#define BTAMP_USE_VOS_WRAPPER
//#undef BTAMP_USE_VOS_WRAPPER
#ifdef BTAMP_USE_VOS_WRAPPER
static BOOL BslProcessACLDataTx
(
    BslClientCtxType* pctx,
    struct sk_buff *skb,
    v_SIZE_t* pCount
)
#else
static BOOL BslProcessACLDataTx
(
    BslClientCtxType* pctx,
    LPCVOID pBuffer_,
    v_SIZE_t* pCount
)
#endif
{
#ifndef BTAMP_USE_VOS_WRAPPER
    LPVOID pBuffer = (LPVOID) pBuffer_; // castaway const-ness of ptr
#endif
    BOOL findPhyStatus;
    BslPhyLinkCtxType* pPhyCtx;
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    WLANTL_ACEnumType Ac;
    hdd_list_node_t* pLink;
    BslTxListNodeType *pNode;
    v_SIZE_t ListSize;
    // I will access the skb in a VOSS packet
#ifndef BTAMP_USE_VOS_WRAPPER
    struct sk_buff *skb;
#endif
#if 0
    static int num_packets;
#endif

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_LOW, "BslProcessACLDataTx" );

    // need to find the PHY link for this ACL data pkt based on phy_link_handle
    // TODO need some endian-ness check?
    ////findPhyStatus = BslFindPhyCtx( pctx, *(v_U8_t *)skb->data, &pPhyCtx );
    //findPhyStatus = BslFindPhyCtx( pctx, *(v_U8_t *)pBuffer, &pPhyCtx );
    // JEZ100604: Temporary short cut
    pPhyCtx = &BslPhyLinkCtx[0];
    findPhyStatus = VOS_TRUE;

    if ( findPhyStatus )
    {
        //Use the skb->cb field to hold the list node information
        pNode = (BslTxListNodeType *) &skb->cb;

        // This list node info includes the VOS pkt
        pNode->skb = skb;

        // stick the SKB into the node
        pLink = (hdd_list_node_t *) pNode;
        VosStatus = WLANBAP_GetAcFromTxDataPkt(pctx->bapHdl, skb->data, &Ac);
        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BslProcessACLDataTx WLANBAP_GetAcFromTxDataPkt "
                 "failed status =%d", VosStatus );

            Ac = WLANTL_AC_BE;
        }

        // now queue the pkt into the correct queue
        // We will want to insert a node of type BslTxListNodeType (was going to be vos_pkt_list_node_t)
        spin_lock_bh(&pPhyCtx->ACLTxQueue[Ac].lock);
        VosStatus = hdd_list_insert_back( &pPhyCtx->ACLTxQueue[Ac], pLink );
        spin_unlock_bh(&pPhyCtx->ACLTxQueue[Ac].lock);

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_ASSERT(0);
        }

        // determine if there is a need to signal TL through BAP
        hdd_list_size( &pPhyCtx->ACLTxQueue[Ac], &ListSize );

        if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
        {
            VOS_ASSERT(0);
        }

        if ( ListSize == 1 )
        {
            // Let TL know we have a packet to send for this AC
            VosStatus = WLANBAP_STAPktPending( pctx->bapHdl, pPhyCtx->PhyLinkHdl, Ac );

            if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
            {
                VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BslProcessACLDataTx WLANBAP_STAPktPending "
                "failed status =%d", VosStatus );
                VOS_ASSERT(0);
            }
        }

        return(TRUE);
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BslProcessACLDataTx attempting to send "
          "data for a non-existant assocation" );

        return(FALSE);
    }


} // BslProcessACLDataTx()


static inline void *hci_get_drvdata(struct hci_dev *hdev)
{
    return hdev->driver_data;
}

static inline void hci_set_drvdata(struct hci_dev *hdev, void *data)
{
    hdev->driver_data = data;
}

/*---------------------------------------------------------------------------
 *   Function definitions
 *-------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------

  \brief BSL_Init() - Initialize the BSL Misc char driver

  This is called in vos_open(), right after WLANBAP_Open(), as part of
  bringing up the BT-AMP PAL (BAP)
  vos_open() will pass in the VOS context. In which a BSL context can be created.

  \param  - NA

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/
//int BSL_Init (void *pCtx)
int BSL_Init ( v_PVOID_t  pvosGCtx )
{
    BslClientCtxType* pctx = NULL;
    ptBtampHandle bapHdl = NULL;        // our handle in BAP
    //ptBtampContext  pBtampCtx = NULL;
    int err = 0;
    struct hci_dev *hdev = NULL;
    //struct net_device *dev = NULL; // Our parent wlan network device
    hdd_adapter_t *pAdapter = NULL;  // Used to retrieve the parent WLAN device
    hdd_context_t *pHddCtx = NULL;
    hdd_config_t *pConfig = NULL;
    hdd_adapter_list_node_t *pAdapterNode = NULL;
    VOS_STATUS status;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BSL_Init");

    /*------------------------------------------------------------------------
      Allocate (and sanity check?!) BSL control block
     ------------------------------------------------------------------------*/
    //vos_alloc_context(pvosGCtx, VOS_MODULE_ID_BSL, (v_VOID_t**)&pctx, sizeof(BslClientCtxType));
    pctx = &BslClientCtx[0];

    bapHdl = vos_get_context( VOS_MODULE_ID_BAP, pvosGCtx);
    if ( NULL == bapHdl )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid BAP pointer from pvosGCtx on BSL_Init");
        return VOS_STATUS_E_FAULT;
    }
    // Save away the btamp (actually the vos) context
    gpCtx = pvosGCtx;

    /* Save away the pointer to the BT-AMP PAL context in the BSL driver context */
    pctx->bapHdl = bapHdl;

    /* Save away the pointer to the BSL driver context in a global (fix this) */
    gpBslctx = pctx;

    /* Initialize all the Phy Contexts to un-used */
    BslInitPhyCtx();

    /* Initialize the Rx fields in the HCI driver context */
    //pctx->rx_state = RECV_WAIT_PACKET_TYPE;
    pctx->rx_count = 0;
    pctx->rx_skb = NULL;

    /* JEZ100713: Temporarily the Tx skb queue will have depth one.*/
    // Don't disturb tx_skb
    //pctx->tx_skb = NULL;
    //pctx->tx_skb = alloc_skb(WLANBAP_MAX_80211_PAL_PDU_SIZE+12, GFP_ATOMIC);

    pctx->hdev = NULL;
    //Get the HDD context.
    pHddCtx = (hdd_context_t *)vos_get_context( VOS_MODULE_ID_HDD, pvosGCtx );
    if(NULL != pHddCtx)
    {
        pConfig = pHddCtx->cfg_ini;
    }
    if(NULL == pConfig)
    {
        VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                  "Didn't register as HCI device");
        return 0;
    }
    else if(0 == pConfig->enableBtAmp)
    {
        VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                  "Didn't register as HCI device, user option(gEnableBtAmp) is set to 0");
        return 0;
    }

    if (VOS_STA_SAP_MODE == hdd_get_conparam())
    {
        status = hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
        if ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
        {
            if ( WLAN_HDD_SOFTAP == pAdapterNode->pAdapter->device_mode)
            {
                pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_SOFTAP);
            }
            else if (WLAN_HDD_P2P_GO == pAdapterNode->pAdapter->device_mode)
            {
                pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_P2P_GO);
            }
        }
     }
    else
        pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);


    if ( NULL == pAdapter )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid HDD Adapter pointer from pvosGCtx on BSL_Init");
        return VOS_STATUS_E_FAULT;
    }

    /* Save away the pointer to the parent WLAN device in BSL driver context */
    pctx->p_dev = pAdapter->dev;

    /* Initialize HCI device */
    hdev = hci_alloc_dev();
    if (!hdev)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Can't allocate HCI device in BSL_Init");
        return VOS_STATUS_E_FAULT;
    }

    /* Save away the HCI device pointer in the BSL driver context */
    pctx->hdev = hdev;

#if defined HCI_80211 || defined HCI_AMP
#define BUILD_FOR_BLUETOOTH_NEXT_2_6
#else
#undef BUILD_FOR_BLUETOOTH_NEXT_2_6
#endif

#ifdef BUILD_FOR_BLUETOOTH_NEXT_2_6
    /* HCI "bus type" of HCI_VIRTUAL should apply */
    hdev->bus = HCI_VIRTUAL;
    /* Set the dev_type to BT-AMP 802.11 */
#ifdef HCI_80211
    hdev->dev_type = HCI_80211;
#else
    hdev->dev_type = HCI_AMP;
#endif
#ifdef FEATURE_WLAN_BTAMP_UT
    /* For the "real" BlueZ build, DON'T Set the device "quirks" to indicate RAW */
    set_bit(HCI_QUIRK_RAW_DEVICE, &hdev->quirks);
#endif
#else //BUILD_FOR_BLUETOOTH_NEXT_2_6
    /* HCI "bus type" of HCI_VIRTUAL should apply */
    hdev->type = HCI_VIRTUAL;
    /* Set the dev_type to BT-AMP 802.11 */
    //hdev->dev_type = HCI_80211;
    ////hdev->dev_type = HCI_AMP;
    /* For the "temporary" BlueZ build, Set the device "quirks" to indicate RAW */
    set_bit(HCI_QUIRK_RAW_DEVICE, &hdev->quirks);
#endif //BUILD_FOR_BLUETOOTH_NEXT_2_6
    /* Save away the BSL driver pointer in the HCI device context */

    hci_set_drvdata(hdev, pctx);
    /* Set the parent device for this HCI device.  This is our WLAN net_device */
    SET_HCIDEV_DEV(hdev, &pctx->p_dev->dev);

    hdev->open     = BSL_Open;
    hdev->close    = BSL_Close;
    hdev->flush    = BSL_Flush;
    hdev->send     = BSL_Write;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
    hdev->destruct = BSL_Destruct;
    hdev->owner = THIS_MODULE;
#endif
    hdev->ioctl    = BSL_IOControl;


    /* Timeout before it is safe to send the first HCI packet */
    msleep(1000);

    /* Register HCI device */
    err = hci_register_dev(hdev);
    if (err < 0)
    {
        VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                  "Unable to register HCI device, err=%d", err);
        pctx->hdev = NULL;
        hci_free_dev(hdev);
        return -ENODEV;
    }

    pHddCtx->isAmpAllowed = VOS_TRUE;
    return 0;
} // BSL_Init()

/**---------------------------------------------------------------------------

  \brief BSL_Deinit() - De-initialize the BSL Misc char driver

  This is called in by WLANBAP_Close() as part of bringing down the BT-AMP PAL (BAP)

  \param  - NA

  \return - 0 for success non-zero for failure

  --------------------------------------------------------------------------*/

int BSL_Deinit( v_PVOID_t  pvosGCtx )
{
    //int err = 0;
    struct hci_dev *hdev;
    BslClientCtxType* pctx = NULL;

    //pctx = vos_get_context( VOS_MODULE_ID_BSL, pvosGCtx);
    pctx = gpBslctx;

    if ( NULL == pctx )
    {
        //VOS_TRACE( VOS_MODULE_ID_BSL, VOS_TRACE_LEVEL_ERROR,
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid BSL pointer from pvosGCtx on BSL_Init");
        return VOS_STATUS_E_FAULT;
    }

    /* Retrieve the HCI device pointer from the BSL driver context */
    hdev = pctx->hdev;

    if (!hdev)
        return 0;

    /* hci_unregister_dev is called again here, in case user didn't call it */
    /* Unregister device from BlueZ; fcn sends us HCI commands before it returns */
    /* And then the registered hdev->close fcn should be called by BlueZ (BSL_Close) */
    hci_unregister_dev(hdev);
    /* BSL_Close is called again here, in case BlueZ didn't call it */
    BSL_Close(hdev);
    hci_free_dev(hdev);
    pctx->hdev = NULL;

    return 0;
} // BSL_Deinit()


/**
  @brief BSL_Open() - This function opens a device for reading, and writing.
  An application indirectly invokes this function when it calls the fopen()
  system call to open a special device file names.

  @param *hdev : [in] pointer to the open HCI device structure.
  BSL_Init (Device Manager) function creates and stores this HCI
  device context in the BSL context.

  @return
  This function returns a status code.  Negative codes are failures.

  NB: I don't seem to be following this convention.
*/
//static int BSL_Open(struct inode *pInode, struct file *pFile)
static int BSL_Open( struct hci_dev *hdev )
{
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    BslClientCtxType* pctx = (BslClientCtxType *)(hci_get_drvdata(hdev));
    v_U16_t i;
    BOOL rval;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BSL_Open");

    /*  you can only open a btamp device one time */
    if (bBslInited)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "BSL_Open: Already Opened.");
        return -EPERM; /* Operation not permitted */
    }

    if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BSLClientLock already inited");
        // return -EIO;  /* I/O error */
        return 0;
    }

    VosStatus = vos_list_init( &BslPhyLinksDescPool );

    if ( !VOS_IS_STATUS_SUCCESS( VosStatus ) )
    {
        //return -EIO;  /* I/O error */
        return 0;
    }

    // now we need to populate this pool with the free pkt desc from the array
    for ( i=0; i<BSL_MAX_PHY_LINKS; i++ )
    {
        VosStatus = vos_list_insert_front( &BslPhyLinksDescPool, &BslPhyLinksDesc[i].node );
        VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ) );
    }

    // This is redundent.  See the check above on (fp->private_data != NULL)
    bBslInited = TRUE;

    rval = BslFindAndInitClientCtx( &pctx );

    if(rval != TRUE)
    {
        // Where is the clean-up in case the above BslFindAndInitClientCtx() call
        // fails?
        //return -EIO;  /* I/O error */
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BSLFindAndInitClientContext failed");
        return 0;
    }


    /* Let Linux fopen() know everything is all right */
    return 0;
} // BSL_Open()

/**
  @brief BSL_Close() - This function closes a device context created by
  BSL_Open(). May be called more than once during AMP PAL shut down.

  @param *hdev : [in] pointer to the open HCI device structure.
  BSL_Init (Device Manager) function creates and stores this HCI
  device context in the BSL context.

  @return
  TRUE indicates success. FALSE indicates failure.
*/
//static int BSL_Close (struct inode *pInode, struct file *pFile)
static int BSL_Close ( struct hci_dev *hdev )
{
    VOS_STATUS VosStatus = VOS_STATUS_SUCCESS;
    BslClientCtxType* pctx;
    vos_list_node_t* pLink;
    v_U16_t i;
    v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
    hdd_context_t *pHddCtx;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BSL_Close");
    if (NULL != pVosContext)
    {
       pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
       if (NULL != pHddCtx)
       {
          pHddCtx->isAmpAllowed = VOS_FALSE;
       }
    }

    // it may seem there is some risk here because we are using a value
    // passed into us as a pointer. what if this pointer is 0 or points to
    // someplace bad? as it turns out the caller is device manager and not
    // the application. kernel should trap such invalid access but we will check
    // for NULL pointer
    if ( hdev == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BSL_Close: NULL hdev specified");
        return FALSE;
    }

    pctx = (BslClientCtxType *)(hci_get_drvdata(hdev));

    if ( pctx == NULL || !bBslInited)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_LOW, "BSL_Close: %s is not open", hdev->name);
        return TRUE;
    }

    // need to cleanup any per PHY state and the common RX state
    BslReleaseClientCtx( pctx );
    for ( i=0; i<BslPhyLinksDescPool.count; i++ )
    {
        VosStatus = vos_list_remove_front( &BslPhyLinksDescPool, &pLink );
        //nothing to free as the nodes came from BslPhyLinksDesc, which is a static
        //this is needed to allow vos_list_destroy() to go through
    }
    VosStatus = vos_list_destroy( &BslPhyLinksDescPool );

    VOS_ASSERT(VOS_IS_STATUS_SUCCESS( VosStatus ) );


    bBslInited = FALSE;

// The next line is temporary
    return(0);
} //BSL_Close()

/**
  @brief BSL_IOControl() - This function sends a command to a device.

  @param *hdev : [in] pointer to the open HCI device structure.
  @param cmd : [in] I/O control operation to perform. These codes are
  device-specific and are usually exposed to developers through a header file.
  @param arg : [in] Additional input parameter.

  @return
  TRUE indicates success. FALSE indicates failure.
*/
//static long BSL_IOControl(struct file *pFile, unsigned int cmd, unsigned long arg)
static int BSL_IOControl(struct hci_dev *hdev, unsigned int cmd, unsigned long arg)
{
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BSL_IOControl - not supported");
    return(TRUE);
} // BSL_IOControl()

/**
  @brief BSL_Flush() - This function flushes all pending commands on a device.

  @param *hdev : [in] pointer to the open HCI device structure.

  @return
  TRUE indicates success. FALSE indicates failure.
*/
static int BSL_Flush(struct hci_dev *hdev)
{
    VOS_STATUS VosStatus;
    BslPhyLinkCtxType* pPhyCtx;

    //VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BSL_Flush - will flush ALL Tx Queues");
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "%s - will flush ALL Tx Queues", __func__);

    /* Flush the TX queue */
    // JEZ100604: Temporary short cut
    pPhyCtx = &BslPhyLinkCtx[0];

    VosStatus = BslFlushTxQueues ( pPhyCtx);

    //return(TRUE);
    return(0);
} // BSL_Flush()

/**
  @brief BSL_Destruct() - This function destroys an HCI device.

  @param *hdev : [in] pointer to the open HCI device structure.

  @return
  TRUE indicates success. FALSE indicates failure.
*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static void BSL_Destruct(struct hci_dev *hdev)
{
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BSL_Destruct - not supported");
    return; //(TRUE);
} // BSL_Destruct()
#endif


/**
  @brief BSL_Write() - This function writes data to the device.
  An application indirectly invokes this function when it calls the fwrite()
  system call to write to a special device file.

  @param *skb : [in] pointer to the skb being transmitted. This skb contains
  the HCI command or HCI data.  Also a pointer (hdev) to the HCI device struct

  @return
  The number of bytes written indicates success.
  Negative values indicate various failures.
*/
//static ssize_t BSL_Write(struct file *pFile, const char __user *pBuffer,
//                         size_t Count, loff_t *pOff)
static int BSL_Write(struct sk_buff *skb)
{
    struct hci_dev *hdev;
    BslClientCtxType* pctx;
    v_SIZE_t written = 0;
    BOOL status;
    //char *bslBuff = NULL;
    BslHciWorkStructure *pHciContext;

    //VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s", __func__);

    // Sanity check inputs
    if ( skb == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: skb is bad i/p", __func__);
        //return -EFAULT; /* Bad address */
        return 0;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: Count (skb->len)=%d", __func__, skb->len);

    // Sanity check inputs
    if ( 0 == skb->len )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: skb is empty", __func__);
        //return -EFAULT; /* Bad address */
        return 0;
    }

    hdev = (struct hci_dev *)(skb->dev);

    // Sanity check the HCI device in the skb
    if ( hdev == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: Frame for Unknown HCI device (hdev=NULL)", __func__);
        //return -ENODEV; /* no device */
        return 0;
    }

    pctx = (BslClientCtxType *)hci_get_drvdata(hdev);

    // Sanity check inputs
    if ( pctx == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: pctx is bad i/p", __func__);
        //return -EFAULT; /* Bad address */
        return 0;
        /* Maybe I should return "no device" */
        //return -ENODEV; /* no device */
    }

    // Switch for each case of packet type
    switch (bt_cb(skb)->pkt_type)
    {
    case HCI_ACLDATA_PKT:
        // Directly execute the data write
        VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                  "%s: HCI ACL data tx, skb=%p",
                  __func__, skb);
        // ACL data
        hdev->stat.acl_tx++;
        // Correct way of doing this...
        written = skb->len;
#ifdef BTAMP_USE_VOS_WRAPPER
        status = BslProcessACLDataTx( pctx, skb, &written );
#else
        status = BslProcessACLDataTx( pctx, skb->data, &written );
        // Free up the skb
        kfree_skb(skb);
#endif //BTAMP_USE_VOS_WRAPPER
        break;
    case HCI_COMMAND_PKT:
        // Defer the HCI command writes
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: HCI command", __func__);
        hdev->stat.cmd_tx++;

        // Allocate an HCI context. To use as a "container" for the "work" to be deferred.
        pHciContext = kmalloc(sizeof(*pHciContext), GFP_ATOMIC);
        if (NULL == pHciContext)
        {
            // no memory for HCI context.  Nothing we can do but drop
            VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                      "%s: Unable to allocate context", __func__);
            kfree_skb(skb);
            return 0;
        }

        // save away the tx skb in the HCI context...so it can be
        // retrieved by the work procedure.
        pHciContext->tx_skb = skb;
        // save away the pctx context...so it can be retrieved by the work procedure.
        pHciContext->pctx = pctx;
        pHciContext->magic = BT_AMP_HCI_CTX_MAGIC;
        INIT_WORK(&pHciContext->hciInterfaceProcessing,
                  bslWriteFinish);

        VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                  "%s: Scheduling work for skb %p, BT-AMP Client context %p, work %p",
                  __func__, skb, pctx, pHciContext);

        status = schedule_work(&pHciContext->hciInterfaceProcessing);

        // Check result
        if ( 0 == status )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "%s: hciInterfaceProcessing work already queued. This should never happen.", __func__);
        }


        // Temporary way of doing this
        //written = skb->len-CMD_TLV_TYPE_AND_LEN_SIZE;
        written = skb->len;
        break;
    case HCI_SCODATA_PKT:
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: unknown type", __func__);
        hdev->stat.sco_tx++;
        // anything else including HCI events and SCO data
        status = FALSE;
        // Free up the skb
        kfree_skb(skb);
        break;
    default:
        // anything else including HCI events and SCO data
        status = FALSE;
        // Free up the skb
        kfree_skb(skb);
        break;
    };


    // JEZ100809: For the HCI command, will the caller need to wait until the work takes place and
    // return the ACTUAL amount of data written.

// The next line is temporary
    //written = skb->len;
    return(written);
} // BSL_Write()

/**
  @brief bslWriteFinish() - This function finished the writes operation
  started by BSL_Write().

  @param work     : [in]  pointer to work structure

  @return         : void

*/
static void bslWriteFinish(struct work_struct *work)
{
    //BslClientCtxType* pctx =
    //    container_of(work, BslClientCtxType, hciInterfaceProcessing);
    BslHciWorkStructure *pHciContext =
        container_of(work, BslHciWorkStructure, hciInterfaceProcessing);
    BslClientCtxType* pctx = pHciContext->pctx;
    VOS_STATUS status;
    struct sk_buff *skb;
    struct hci_dev *hdev;
    //char *bslBuff = NULL;
    v_SIZE_t written = 0;

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_LOW,
              "%s: Entered, context %p",
              __func__, pctx);

    // Sanity check inputs
    if ( pctx == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: pctx is bad i/p", __func__);
        return; // -EFAULT; /* Bad address */
    }

    //skb = pctx->tx_skb;
    skb = pHciContext->tx_skb;
    kfree( pHciContext);

    // Sanity check inputs
    if ( skb == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: skb is bad i/p", __func__);
        return; // -EFAULT; /* Bad address */
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: Count (skb->len)=%d", __func__, skb->len);

    hdev = (struct hci_dev *)(skb->dev);

    // Sanity check the HCI device in the skb
    if ( hdev == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: Frame for Unknown HCI device (hdev=NULL)", __func__);
        return; // -ENODEV; /* no device */
    }


    // Sanity check inputs
    if ( pctx != (BslClientCtxType *)hci_get_drvdata(hdev));
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: pctx and hdev not consistent - bad i/p", __func__);
        return; // -EFAULT; /* Bad address */
        /* Maybe I should return "no device" */
        //return -ENODEV; /* no device */
    }

    // Switch for each case of packet type
    switch (bt_cb(skb)->pkt_type)
    {
    case HCI_COMMAND_PKT:
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: HCI command", __func__);
        hdev->stat.cmd_tx++;
        // HCI command
        status = BslProcessHCICommand( pctx, skb->data, skb->len-CMD_TLV_TYPE_AND_LEN_SIZE);
        // Temporary way of doing this
        //written = skb->len-CMD_TLV_TYPE_AND_LEN_SIZE;
        written = skb->len;
        break;
    case HCI_SCODATA_PKT:
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: unknown type", __func__);
        hdev->stat.sco_tx++;
        // anything else including HCI events and SCO data
        status = FALSE;
        break;
    default:
        // anything else including HCI events and SCO data
        status = FALSE;
        break;
    };

    VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
              "%s: Freeing skb %p",
              __func__, skb);

    consume_skb(skb);

// How do I return the actual number of bytes written to the caller?
//   return(written);
    return;
} //bslWriteFinish()

VOS_STATUS WLANBAP_SetConfig
(
    WLANBAP_ConfigType *pConfig
)
{
    BslClientCtxType* pctx;
    VOS_STATUS status;
    // sanity checking
    if ( pConfig == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_SetConfig bad input" );
        return VOS_STATUS_E_FAILURE;
    }
    pctx = gpBslctx;
    if ( NULL == pctx )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid BSL pointer from pctx on WLANBAP_SetConfig");
        return VOS_STATUS_E_FAULT;
    }

    // get a handle from BAP
    status = WLANBAP_GetNewHndl(&pctx->bapHdl);
    if ( !VOS_IS_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_SetConfig can't get BAP handle" );
        return VOS_STATUS_E_FAILURE;
    }


    status = WLAN_BAPSetConfig(pctx->bapHdl, pConfig);
    if ( !VOS_IS_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "WLANBAP_SetConfig can't set BAP config" );
        return VOS_STATUS_E_FAILURE;
    }

    return(VOS_STATUS_SUCCESS);
}

VOS_STATUS WLANBAP_RegisterWithHCI(hdd_adapter_t *pAdapter)
{
    struct hci_dev *hdev = NULL;
    BslClientCtxType* pctx = NULL;
    int err = 0;
    v_CONTEXT_t pVosContext = vos_get_global_context( VOS_MODULE_ID_HDD, NULL );
    hdd_context_t *pHddCtx;

    pctx = gpBslctx;

    if ( NULL == pctx )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid BSL pointer from pctx on WLANBAP_RegisterWithHCI");
        return VOS_STATUS_E_FAULT;
    }
    if ( NULL == pAdapter )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid HDD Adapter pointer from pvosGCtx on WLANBAP_RegisterWithHCI");
        return VOS_STATUS_E_FAULT;
    }

    if(NULL != pctx->hdev)
    {
        VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_WARN,
                  "Already registered as HCI device");
        return VOS_STATUS_SUCCESS;
    }



    /* Save away the pointer to the parent WLAN device in BSL driver context */
    pctx->p_dev = pAdapter->dev;

    /* Initialize HCI device */
    hdev = hci_alloc_dev();
    if (!hdev)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Can't allocate HCI device in WLANBAP_RegisterWithHCI");
        return VOS_STATUS_E_FAULT;
    }

    /* Save away the HCI device pointer in the BSL driver context */
    pctx->hdev = hdev;

#if defined HCI_80211 || defined HCI_AMP
#define BUILD_FOR_BLUETOOTH_NEXT_2_6
#else
#undef BUILD_FOR_BLUETOOTH_NEXT_2_6
#endif

#ifdef BUILD_FOR_BLUETOOTH_NEXT_2_6
    /* HCI "bus type" of HCI_VIRTUAL should apply */
    hdev->bus = HCI_VIRTUAL;
    /* Set the dev_type to BT-AMP 802.11 */
#ifdef HCI_80211
    hdev->dev_type = HCI_80211;
#else
    hdev->dev_type = HCI_AMP;
#endif
#ifdef FEATURE_WLAN_BTAMP_UT
    /* For the "real" BlueZ build, DON'T Set the device "quirks" to indicate RAW */
    set_bit(HCI_QUIRK_RAW_DEVICE, &hdev->quirks);
#endif
#else //BUILD_FOR_BLUETOOTH_NEXT_2_6
    /* HCI "bus type" of HCI_VIRTUAL should apply */
    hdev->type = HCI_VIRTUAL;
    /* Set the dev_type to BT-AMP 802.11 */
    //hdev->dev_type = HCI_80211;
    ////hdev->dev_type = HCI_AMP;
    /* For the "temporary" BlueZ build, Set the device "quirks" to indicate RAW */
    set_bit(HCI_QUIRK_RAW_DEVICE, &hdev->quirks);
#endif //BUILD_FOR_BLUETOOTH_NEXT_2_6
    /* Save away the BSL driver pointer in the HCI device context */
    hci_set_drvdata(hdev, pctx);
    /* Set the parent device for this HCI device.  This is our WLAN net_device */
    SET_HCIDEV_DEV(hdev, &pctx->p_dev->dev);

    hdev->open     = BSL_Open;
    hdev->close    = BSL_Close;
    hdev->flush    = BSL_Flush;
    hdev->send     = BSL_Write;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
    hdev->owner = THIS_MODULE;
    hdev->destruct = BSL_Destruct;
#endif
    hdev->ioctl    = BSL_IOControl;


    /* Timeout before it is safe to send the first HCI packet */
    msleep(1000);

    /* Register HCI device */
    err = hci_register_dev(hdev);
    if (err < 0)
    {
        VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                  "Unable to register HCI device, err=%d", err);
        pctx->hdev = NULL;
        hci_free_dev(hdev);
        return VOS_STATUS_E_FAULT;
    }
    if (NULL != pVosContext)
    {
       pHddCtx = vos_get_context( VOS_MODULE_ID_HDD, pVosContext);
       if (NULL != pHddCtx)
       {
          pHddCtx->isAmpAllowed = VOS_TRUE;
       }
    }

    return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANBAP_DeregisterFromHCI(void)
{
    struct hci_dev *hdev;
    BslClientCtxType* pctx = NULL;

    pctx = gpBslctx;

    if ( NULL == pctx )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid BSL pointer from pvosGCtx on WLANBAP_DeregisterFromHCI");
        return VOS_STATUS_E_FAULT;
    }

    /* Retrieve the HCI device pointer from the BSL driver context */
    hdev = pctx->hdev;

    if (!hdev)
        return VOS_STATUS_E_FAULT;

    /* Unregister device from BlueZ; fcn sends us HCI commands before it returns */
    /* And then the registered hdev->close fcn should be called by BlueZ (BSL_Close) */
    hci_unregister_dev(hdev);

    /* BSL_Close is called again here, in case BlueZ didn't call it */
    BSL_Close(hdev);
    hci_free_dev(hdev);
    pctx->hdev = NULL;

    return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANBAP_StopAmp(void)
{
    BslClientCtxType* pctx;
    VOS_STATUS status = VOS_STATUS_SUCCESS;

    pctx = gpBslctx;

    if(NULL == pctx)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid BSL pointer from pvosGCtx on WLANBAP_StopAmp");
        status = VOS_STATUS_E_FAULT;
    }
    else
    {
        //is AMP session on, if so disconnect
        if(VOS_TRUE == WLAN_BAPSessionOn(pctx->bapHdl))
        {
            status = WLAN_BAPDisconnect(pctx->bapHdl);
        }
    }
    return status;
}

v_BOOL_t WLANBAP_AmpSessionOn(void)
{
    BslClientCtxType* pctx;

    pctx = gpBslctx;
    if(NULL == pctx)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid BSL pointer from pvosGCtx on WLANBAP_AmpSessionOn");
        return VOS_FALSE;
    }
    else
    {
        return( WLAN_BAPSessionOn(pctx->bapHdl));
    }
}


#endif // WLAN_BTAMP_FEATURE
