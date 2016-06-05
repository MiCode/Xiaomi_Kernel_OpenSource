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




/*===========================================================================


                       W L A N _ Q C T _ T L . C

  OVERVIEW:

  This software unit holds the implementation of the WLAN Transport Layer.

  The functions externalized by this module are to be called ONLY by other
  WLAN modules that properly register with the Transport Layer initially.

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
2010-07-13    c_shinde Fixed an issue where WAPI rekeying was failing because 
                      WAI frame sent out during rekeying had the protected bit
                      set to 1.
2010-05-06    rnair   Changed name of variable from usLlcType to usEtherType
                      Changed function name from GetLLCType to GetEtherType
                      Fixed 802.3 to 802.11 frame translation issue where two 
                      bytes of the LLC header was getting overwritten in the
                      non-Qos path
2010-05-06    rnair   RxAuth path fix for modifying the header before ether
                      type is retreived (Detected while testing rekeying
                      in WAPI Volans)
2010-02-19    bad     Fixed 802.11 to 802.3 ft issues with WAPI
2010-02-19    rnair   WAPI: If frame is a WAI frame in TxConn and TxAuth, TL 
                      does frame translation. 
2010-02-01    rnair   WAPI: Fixed a bug where the value of ucIsWapiSta was not       
                      being set in the TL control block in the RegisterSTA func. 
2010-01-08    lti     Added TL Data Caching 
2009-11-04    rnair   WAPI: Moving common functionality to a seperate function
                      called WLANTL_GetLLCType
2009-10-15    rnair   WAPI: Featurizing WAPI code
2009-10-09    rnair   WAPI: Modifications to authenticated state handling of Rx data
2009-10-06    rnair   Adding support for WAPI 
2009-09-22    lti     Add deregistration API for management client
2009-07-16    rnair   Temporary fix to let TL fetch packets when multiple 
                      peers exist in an IBSS
2009-06-10    lti     Fix for checking TID value of meta info on TX - prevent
                      memory overwrite 
                      Fix for properly checking the sta id for resuming trigger
                      frame generation
2009-05-14    lti     Fix for sending out trigger frames
2009-05-15    lti     Addr3 filtering 
2009-04-13    lti     Assert if packet larger then allowed
                      Drop packet that fails flatten
2009-04-02    lti     Performance fixes for TL 
2009-02-19    lti     Added fix for LLC management on Rx Connect 
2009-01-16    lti     Replaced peek data with extract data for non BD opertions
                      Extracted frame control in Tl and pass to HAL for frame 
                      type evaluation
2009-02-02    sch     Add handoff support
2008-12-09    lti     Fixes for AMSS compilation 
                      Removed assert on receive when there is no station
2008-12-02    lti     Fix fo trigger frame generation 
2008-10-31    lti     Fix fo TL tx suspend
2008-10-01    lti     Merged in fixes from reordering
                      Disabled part of UAPSD functionality in TL
                      (will be re-enabled once UAPSD is tested)
                      Fix for UAPSD multiple enable
2008-08-10    lti     Fixes following UAPSD testing
                      Fixed infinite loop on mask computation when STA no reg
2008-08-06    lti     Fixes after QOS unit testing
2008-08-06    lti     Added QOS support
2008-07-23    lti     Fix for vos packet draining
2008-07-17    lti     Fix for data type value
                      Added frame translation code in TL
                      Avoid returning failure to PE in case previous frame is
                      still pending; fail previous and cache new one for tx
                      Get frames returning boolean true if more frames are pending
2008-07-03    lti     Fixes following pre-integration testing
2008-06-26    lti     Fixes following unit testing
                      Added alloc and free for TL context
                      Using atomic set u8 instead of u32
2008-05-16    lti     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "wlan_qct_tl.h" 
#include "wlan_qct_wda.h" 
#include "wlan_qct_tli.h" 
#include "wlan_qct_tli_ba.h" 
#include "wlan_qct_tl_hosupport.h"
#include "vos_types.h"
#include "vos_trace.h"
#include "wlan_qct_tl_trace.h"
#include "tlDebug.h"
#include "cfgApi.h"
#ifdef FEATURE_WLAN_WAPI
/*Included to access WDI_RxBdType */
#include "wlan_qct_wdi_bd.h"
#endif
/*Enables debugging behavior in TL*/
#define TL_DEBUG
/*Enables debugging FC control frame in TL*/
//#define TL_DEBUG_FC
//#define WLAN_SOFTAP_FLOWCTRL_EN
//#define BTAMP_TEST
#ifdef TL_DEBUG_FC
#include <wlan_qct_pal_status.h>
#include <wlan_qct_pal_device.h> // wpalReadRegister
#endif

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
/*LLC header value*/
static v_U8_t WLANTL_LLC_HEADER[] =  {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00 };

#ifdef FEATURE_WLAN_ESE
/*Aironet SNAP header value*/
static v_U8_t WLANTL_AIRONET_SNAP_HEADER[] =  {0xAA, 0xAA, 0x03, 0x00, 0x40, 0x96, 0x00, 0x00 };
#endif //FEATURE_WLAN_ESE

/*BT-AMP packet LLC OUI value*/
const v_U8_t WLANTL_BT_AMP_OUI[] =  {0x00, 0x19, 0x58 };

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
extern const v_U8_t  WLANTL_TID_2_AC[WLAN_MAX_TID];

#endif

#define WLANTL_MAX_SNR_DATA_SAMPLES 20

#ifdef VOLANS_PERF
#define WLANTL_BD_PDU_INTERRUPT_ENABLE_THRESHOLD  120
#define WLANTL_BD_PDU_INTERRUPT_GET_THRESHOLD  120

/* TL BD/PDU threshold to enable interrupt */
int bdPduInterruptEnableThreshold = WLANTL_BD_PDU_INTERRUPT_ENABLE_THRESHOLD;
int bdPduInterruptGetThreshold = WLANTL_BD_PDU_INTERRUPT_GET_THRESHOLD;
#endif /* VOLANS_PERF */

/*-----------------------------------*
 |   Type(2b)   |     Sub-type(4b)   |
 *-----------------------------------*/
#define WLANTL_IS_DATA_FRAME(_type_sub)                               \
                     ( WLANTL_DATA_FRAME_TYPE == ( (_type_sub) & 0x30 ))

#define WLANTL_IS_QOS_DATA_FRAME(_type_sub)                                      \
                     (( WLANTL_DATA_FRAME_TYPE == ( (_type_sub) & 0x30 )) &&     \
                      ( WLANTL_80211_DATA_QOS_SUBTYPE == ( (_type_sub) & 0xF ))) 

#define WLANTL_IS_MGMT_FRAME(_type_sub)                                     \
                     ( WLANTL_MGMT_FRAME_TYPE == ( (_type_sub) & 0x30 ))

#define WLANTL_IS_MGMT_ACTION_FRAME(_type_sub)                                \
    (( WLANTL_MGMT_FRAME_TYPE == ( (_type_sub) & 0x30 )) &&    \
     ( ( WLANTL_80211_MGMT_ACTION_SUBTYPE == ( (_type_sub) & 0xF )) || \
       ( WLANTL_80211_MGMT_ACTION_NO_ACK_SUBTYPE == ( (_type_sub) & 0xF ))))

#define WLANTL_IS_PROBE_REQ(_type_sub)                                     \
                     ( WLANTL_MGMT_PROBE_REQ_FRAME_TYPE == ( (_type_sub) & 0x3F ))

#define WLANTL_IS_CTRL_FRAME(_type_sub)                                     \
                     ( WLANTL_CTRL_FRAME_TYPE == ( (_type_sub) & 0x30 ))

#ifdef FEATURE_WLAN_TDLS
#define WLANTL_IS_TDLS_FRAME(_eth_type)                                     \
                     ( WLANTL_LLC_TDLS_TYPE == ( _eth_type))
#endif

/*MAX Allowed len processed by TL - MAx MTU + 802.3 header + BD+DXE+XTL*/
#define WLANTL_MAX_ALLOWED_LEN    (1514 + 100)

#define WLANTL_DATA_FLOW_MASK 0x0F

//some flow_control define
//LWM mode will be enabled for this station if the egress/ingress falls below this ratio
#define WLANTL_LWM_EGRESS_INGRESS_THRESHOLD (0.75)

//Get enough sample to do the LWM related calculation
#define WLANTL_LWM_INGRESS_SAMPLE_THRESHOLD (64)

//Maximal on-fly packet per station in LWM mode
#define WLANTL_STA_BMU_THRESHOLD_MAX (256)

#define WLANTL_AC_MASK (0x7)
#define WLANTL_STAID_OFFSET (0x6)

/* UINT32 type endian swap */
#define SWAP_ENDIAN_UINT32(a)          ((a) = ((a) >> 0x18 ) |(((a) & 0xFF0000) >> 0x08) | \
                                            (((a) & 0xFF00) << 0x08)  | (((a) & 0xFF) << 0x18))

/* Maximum value of SNR that can be calculated by the HW */
#define WLANTL_MAX_HW_SNR 35

#define DISABLE_ARP_TOGGLE 0
#define ENABLE_ARP_TOGGLE  1
#define SEND_ARP_ON_WQ5    2

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
#define TL_LITTLE_BIT_ENDIAN

typedef struct
{


   v_U8_t protVer :2;
   v_U8_t type :2;
   v_U8_t subType :4;

   v_U8_t toDS :1;
   v_U8_t fromDS :1;
   v_U8_t moreFrag :1;
   v_U8_t retry :1;
   v_U8_t powerMgmt :1;
   v_U8_t moreData :1;
   v_U8_t wep :1;
   v_U8_t order :1;


} WLANTL_MACFCType;

/* 802.11 header */
typedef struct
{
 /* Frame control field */
 WLANTL_MACFCType  wFrmCtrl;

 /* Duration ID */
 v_U16_t  usDurationId;

 /* Address 1 field  */
 v_U8_t   vA1[VOS_MAC_ADDR_SIZE];

 /* Address 2 field */
 v_U8_t   vA2[VOS_MAC_ADDR_SIZE];

 /* Address 3 field */
 v_U8_t   vA3[VOS_MAC_ADDR_SIZE];

 /* Sequence control field */
 v_U16_t  usSeqCtrl;

 // Find the size of the mandatory header size.
#define WLAN80211_MANDATORY_HEADER_SIZE \
    (sizeof(WLANTL_MACFCType) + sizeof(v_U16_t) + \
    (3 * (sizeof(v_U8_t) * VOS_MAC_ADDR_SIZE))  + \
    sizeof(v_U16_t))

 /* Optional A4 address */
 v_U8_t   optvA4[VOS_MAC_ADDR_SIZE];

 /* Optional QOS control field */
 v_U16_t  usQosCtrl;
}WLANTL_80211HeaderType;

/* 802.3 header */
typedef struct
{
 /* Destination address field */
 v_U8_t   vDA[VOS_MAC_ADDR_SIZE];

 /* Source address field */
 v_U8_t   vSA[VOS_MAC_ADDR_SIZE];

 /* Length field */
 v_U16_t  usLenType;
}WLANTL_8023HeaderType;

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/
#define WLAN_TL_INVALID_U_SIG 255
#define WLAN_TL_INVALID_B_SIG 255
#define ENTER() VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO, "Enter:%s", __func__)

#define WLAN_TL_AC_ARRAY_2_MASK( _pSTA, _ucACMask, i ) \
  do\
  {\
    _ucACMask = 0; \
    for ( i = 0; i < WLANTL_NUM_TX_QUEUES; i++ ) \
    { \
      if ( 0 != (_pSTA)->aucACMask[i] ) \
      { \
        _ucACMask |= ( 1 << i ); \
      } \
    } \
  } while (0);

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

static VOS_STATUS 
WLANTL_GetEtherType
(
  v_U8_t               * aucBDHeader,
  vos_pkt_t            * vosDataBuff,
  v_U8_t                 ucMPDUHLen,
  v_U16_t              * usEtherType
);


/*----------------------------------------------------------------------------
 * Externalized Function Definitions
* -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/
/*==========================================================================

  FUNCTION        WLANTL_FreeClientMemory

  DESCRIPTION
  It frees up the memory allocated to all the STA clients in TLCB block
  Can be called inside Close, Stop or when some FAULT occurs

  DEPENDENCIES

  PARAMETERS

  IN
  pClientSTA:    Pointer to the global client pointer array

  RETURN VALUE

  SIDE EFFECTS

============================================================================*/
void WLANTL_FreeClientMemory
(WLANTL_STAClientType* pClientSTA[WLAN_MAX_STA_COUNT])
{
    v_U32_t i = 0;
    for(i =0; i < WLAN_MAX_STA_COUNT; i++)
    {
        if( NULL != pClientSTA[i] )
        {
           vos_mem_free(pClientSTA[i]);
        }
        pClientSTA[i] = NULL;
    }
    return;
}

/*==========================================================================

  FUNCTION    WLANTL_Open

  DESCRIPTION
    Called by HDD at driver initialization. TL will initialize all its
    internal resources and will wait for the call to start to register
    with the other modules.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    pTLConfig:      TL Configuration

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_Open
(
  v_PVOID_t               pvosGCtx,
  WLANTL_ConfigInfoType*  pTLConfig
)
{
  WLANTL_CbType*  pTLCb = NULL; 
  v_U8_t          ucIndex; 
  tHalHandle      smeContext;
  v_U32_t i = 0;
#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
  VOS_STATUS      status = VOS_STATUS_SUCCESS;
#endif
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  ENTER();
  vos_alloc_context( pvosGCtx, VOS_MODULE_ID_TL, 
                    (void*)&pTLCb, sizeof(WLANTL_CbType));

  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if (( NULL == pTLCb ) || ( NULL == pTLConfig ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
               "WLAN TL: Invalid input pointer on WLANTL_Open TL %p Config %p", pTLCb, pTLConfig ));
    return VOS_STATUS_E_FAULT;
  }

  /* Set the default log level to VOS_TRACE_LEVEL_ERROR */
  vos_trace_setLevel(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR);

  smeContext = vos_get_context(VOS_MODULE_ID_SME, pvosGCtx);
  if ( NULL == smeContext )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid smeContext", __func__));
    return VOS_STATUS_E_FAULT;
  }

  /* Zero out the memory so we are OK, when CleanCB is called.*/
  vos_mem_zero((v_VOID_t *)pTLCb, sizeof(WLANTL_CbType));

  /*------------------------------------------------------------------------
    Clean up TL control block, initialize all values
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:WLANTL_Open"));

  for ( i =0; i<WLAN_MAX_STA_COUNT; i++ )
  {
      if ( i < WLAN_NON32_STA_COUNT )
      {
          pTLCb->atlSTAClients[i] = vos_mem_malloc(sizeof(WLANTL_STAClientType));
          /* Allocating memory for LEGACY STA COUNT so as to avoid regression issues.  */
         if ( NULL == pTLCb->atlSTAClients[i] )
         {
             TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR, "WLAN TL: StaClient allocation failed"));
             WLANTL_FreeClientMemory(pTLCb->atlSTAClients);
             vos_free_context(pvosGCtx, VOS_MODULE_ID_TL, pTLCb);
             return VOS_STATUS_E_FAULT;
         }
         vos_mem_zero((v_VOID_t *) pTLCb->atlSTAClients[i], sizeof(WLANTL_STAClientType));
      }
      else
      {
          pTLCb->atlSTAClients[i] = NULL;
      }
  }

  pTLCb->reorderBufferPool = vos_mem_vmalloc(sizeof(WLANTL_REORDER_BUFFER_T) * WLANTL_MAX_BA_SESSION);
  if (NULL == pTLCb->reorderBufferPool)
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR, "WLAN TL: Reorder buffer allocation failed"));
    WLANTL_FreeClientMemory(pTLCb->atlSTAClients);
    vos_free_context(pvosGCtx, VOS_MODULE_ID_TL, pTLCb);
    return VOS_STATUS_E_FAULT;
  }

  vos_mem_zero((v_VOID_t *)pTLCb->reorderBufferPool, sizeof(WLANTL_REORDER_BUFFER_T) * WLANTL_MAX_BA_SESSION);

  WLANTL_CleanCB(pTLCb, 0 /*do not empty*/);

  for ( ucIndex = 0; ucIndex < WLANTL_NUM_TX_QUEUES ; ucIndex++)
  {
    pTLCb->tlConfigInfo.ucAcWeights[ucIndex] = pTLConfig->ucAcWeights[ucIndex];
  }

  for ( ucIndex = 0; ucIndex < WLANTL_MAX_AC ; ucIndex++)
  {
    pTLCb->tlConfigInfo.ucReorderAgingTime[ucIndex] = pTLConfig->ucReorderAgingTime[ucIndex];
  }

  // scheduling init to be the last one of previous round
  pTLCb->uCurServedAC = WLANTL_AC_BK;
  pTLCb->ucCurLeftWeight = 1;
  pTLCb->ucCurrentSTA = WLAN_MAX_STA_COUNT-1;

#if 0
  //flow control field init
  vos_mem_zero(&pTLCb->tlFCInfo, sizeof(tFcTxParams_type));
  //bit 0: set (Bd/pdu count) bit 1: set (request station PS change notification)
  pTLCb->tlFCInfo.fcConfig = 0x1;
#endif

  pTLCb->vosTxFCBuf = NULL;
  pTLCb->tlConfigInfo.uMinFramesProcThres =
                pTLConfig->uMinFramesProcThres;

#ifdef FEATURE_WLAN_TDLS
  pTLCb->ucTdlsPeerCount = 0;
#endif

  pTLCb->tlConfigInfo.uDelayedTriggerFrmInt =
                pTLConfig->uDelayedTriggerFrmInt;

  /*------------------------------------------------------------------------
    Allocate internal resources
   ------------------------------------------------------------------------*/
  vos_pkt_get_packet(&pTLCb->vosDummyBuf, VOS_PKT_TYPE_RX_RAW, 1, 1,
                     1/*true*/,NULL, NULL);

  WLANTL_InitBAReorderBuffer(pvosGCtx);
#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
   /* Initialize Handoff support modue
    * RSSI measure and Traffic state monitoring */
  status = WLANTL_HSInit(pvosGCtx);
  if(!VOS_IS_STATUS_SUCCESS(status))
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "Handoff support module init fail"));
    WLANTL_FreeClientMemory(pTLCb->atlSTAClients);
    vos_mem_vfree(pTLCb->reorderBufferPool);
    vos_free_context(pvosGCtx, VOS_MODULE_ID_TL, pTLCb);
    return status;
  }
#endif

  pTLCb->isBMPS = VOS_FALSE;
  pmcRegisterDeviceStateUpdateInd( smeContext,
                                   WLANTL_PowerStateChangedCB, pvosGCtx );

  return VOS_STATUS_SUCCESS;
}/* WLANTL_Open */

/*==========================================================================

  FUNCTION    WLANTL_Start

  DESCRIPTION
    Called by HDD as part of the overall start procedure. TL will use this
    call to register with BAL as a transport layer entity.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS:  Everything is good :)

    Other codes can be returned as a result of a BAL failure; see BAL API
    for more info

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_Start
(
  v_PVOID_t  pvosGCtx
)
{
  WLANTL_CbType*      pTLCb      = NULL;
  v_U32_t             uResCount = WDA_TLI_MIN_RES_DATA;
  VOS_STATUS          vosStatus;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  ENTER();
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_Start"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Register with WDA as transport layer client
    Request resources for tx from bus
  ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:WLAN TL:WLANTL_Start"));

  tlTraceInit();
  vosStatus = WDA_DS_Register( pvosGCtx, 
                          WLANTL_TxComp, 
                          WLANTL_RxFrames,
                          WLANTL_GetFrames, 
                          WLANTL_ResourceCB,
                          WDA_TLI_MIN_RES_DATA, 
                          pvosGCtx, 
                          &uResCount ); 

  if ( VOS_STATUS_SUCCESS != vosStatus )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR, 
               "WLAN TL:TL failed to register with BAL/WDA, Err: %d",
               vosStatus));
    return vosStatus;
  }

  /* Enable transmission */
  vos_atomic_set_U8( &pTLCb->ucTxSuspended, 0);

  pTLCb->uResCount = uResCount;
  return VOS_STATUS_SUCCESS;
}/* WLANTL_Start */

/*==========================================================================

  FUNCTION    WLANTL_Stop

  DESCRIPTION
    Called by HDD to stop operation in TL, before close. TL will suspend all
    frame transfer operation and will wait for the close request to clean up
    its resources.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_Stop
(
  v_PVOID_t  pvosGCtx
)
{
  WLANTL_CbType*  pTLCb = NULL;
  v_U8_t      ucIndex;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  ENTER();
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Stop TL and empty Station list
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:WLANTL_Stop"));

  /* Disable transmission */
  vos_atomic_set_U8( &pTLCb->ucTxSuspended, 1);

  if ( NULL != pTLCb->tlMgmtFrmClient.vosPendingDataBuff )
  {
    vos_pkt_return_packet(pTLCb->tlMgmtFrmClient.vosPendingDataBuff);
    pTLCb->tlMgmtFrmClient.vosPendingDataBuff = NULL;
  }

  if ( NULL != pTLCb->tlBAPClient.vosPendingDataBuff )
  {
    vos_pkt_return_packet(pTLCb->tlBAPClient.vosPendingDataBuff);
    pTLCb->tlBAPClient.vosPendingDataBuff = NULL;
  }

#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
  if(VOS_STATUS_SUCCESS != WLANTL_HSStop(pvosGCtx))
  {
    TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
               "Handoff Support module stop fail"));
  }
#endif

  /*-------------------------------------------------------------------------
    Clean client stations
   -------------------------------------------------------------------------*/
  for ( ucIndex = 0; ucIndex < WLAN_MAX_STA_COUNT; ucIndex++)
  {
    if ( NULL != pTLCb->atlSTAClients[ucIndex] )
    {
        WLANTL_CleanSTA(pTLCb->atlSTAClients[ucIndex], 1 /*empty all queues*/);
    }
  }


  return VOS_STATUS_SUCCESS;
}/* WLANTL_Stop */

/*==========================================================================

  FUNCTION    WLANTL_Close

  DESCRIPTION
    Called by HDD during general driver close procedure. TL will clean up
    all the internal resources.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_Close
(
  v_PVOID_t  pvosGCtx
)
{
  WLANTL_CbType*  pTLCb = NULL;
  tHalHandle smeContext;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  ENTER();
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_FAULT;
  }
  /*------------------------------------------------------------------------
    Deregister from PMC
   ------------------------------------------------------------------------*/
  smeContext = vos_get_context(VOS_MODULE_ID_SME, pvosGCtx);
  if ( NULL == smeContext )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid smeContext", __func__));
    // continue so that we can cleanup as much as possible
  }
  else
  {
    pmcDeregisterDeviceStateUpdateInd( smeContext, WLANTL_PowerStateChangedCB );
  }

#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
  if(VOS_STATUS_SUCCESS != WLANTL_HSDeInit(pvosGCtx))
  {
    TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
               "Handoff Support module DeInit fail"));
  }
#endif

  /*------------------------------------------------------------------------
    Cleanup TL control block.
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL: WLANTL_Close"));
  WLANTL_CleanCB(pTLCb, 1 /* empty queues/lists/pkts if any*/);

  WLANTL_FreeClientMemory(pTLCb->atlSTAClients);

  vos_mem_vfree(pTLCb->reorderBufferPool);

  /*------------------------------------------------------------------------
    Free TL context from VOSS global
   ------------------------------------------------------------------------*/
  vos_free_context(pvosGCtx, VOS_MODULE_ID_TL, pTLCb);
  return VOS_STATUS_SUCCESS;
}/* WLANTL_Close */

/*----------------------------------------------------------------------------
    INTERACTION WITH HDD
 ---------------------------------------------------------------------------*/
/*==========================================================================

  FUNCTION    WLANTL_ConfigureSwFrameTXXlationForAll

  DESCRIPTION
     Function to disable/enable frame translation for all association stations.

  DEPENDENCIES

  PARAMETERS
   IN
    pvosGCtx: VOS context 
    EnableFrameXlation TRUE means enable SW translation for all stations.
    .

  RETURN VALUE

   void.

============================================================================*/
void
WLANTL_ConfigureSwFrameTXXlationForAll
(
  v_PVOID_t pvosGCtx, 
  v_BOOL_t enableFrameXlation
)
{
  v_U8_t ucIndex;
  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  WLANTL_CbType* pTLCb = VOS_GET_TL_CB(pvosGCtx);
  WLANTL_STAClientType* pClientSTA = NULL;
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Invalid TL pointer from pvosGCtx on "
           "WLANTL_ConfigureSwFrameTXXlationForAll"));
    return;
  }

  TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
     "WLANTL_ConfigureSwFrameTXXlationForAll: Configure SW frameXlation %d", 
      enableFrameXlation));

  for ( ucIndex = 0; ucIndex < WLAN_MAX_TID; ucIndex++) 
  {
    pClientSTA = pTLCb->atlSTAClients[ucIndex];
    if ( NULL != pClientSTA && 0 != pClientSTA->ucExists )
    {
#ifdef WLAN_SOFTAP_VSTA_FEATURE
      // if this station was not allocated resources to perform HW-based
      // TX frame translation then force SW-based TX frame translation
      // otherwise use the frame translation supplied by the client
      if (!WDA_IsHwFrameTxTranslationCapable(pvosGCtx, ucIndex))
      {
        pClientSTA->wSTADesc.ucSwFrameTXXlation = 1;
      }
      else
#endif
        pClientSTA->wSTADesc.ucSwFrameTXXlation = enableFrameXlation;
    }
  }
}

/*===========================================================================

  FUNCTION    WLANTL_StartForwarding

  DESCRIPTION

    This function is used to ask serialization through TX thread of the   
    cached frame forwarding (if statation has been registered in the mean while) 
    or flushing (if station has not been registered by the time)

    In case of forwarding, upper layer is only required to call WLANTL_RegisterSTAClient()
    and doesn't need to call this function explicitly. TL will handle this inside 
    WLANTL_RegisterSTAClient(). 
    
    In case of flushing, upper layer is required to call this function explicitly
    
  DEPENDENCIES

    TL must have been initialized before this gets called.

   
  PARAMETERS

   ucSTAId:   station id 

  RETURN VALUE

    The result code associated with performing the operation
    Please check return values of vos_tx_mq_serialize.

  SIDE EFFECTS
    If TL was asked to perform WLANTL_CacheSTAFrame() in WLANTL_RxFrames(), 
    either WLANTL_RegisterSTAClient() or this function must be called 
    within reasonable time. Otherwise, TL will keep cached vos buffer until
    one of this function is called, and may end up with system buffer exhasution. 

    It's an upper layer's responsibility to call this function in case of
    flushing

============================================================================*/

VOS_STATUS 
WLANTL_StartForwarding
(
  v_U8_t ucSTAId,
  v_U8_t ucUcastSig, 
  v_U8_t ucBcastSig 
)
{
  vos_msg_t      sMessage;
  v_U32_t        uData;             
 /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Signal the OS to serialize our event */
  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
             "Serializing TL Start Forwarding Cached for control STA %d", 
              ucSTAId );

  vos_mem_zero( &sMessage, sizeof(vos_msg_t) );

  uData = ucSTAId | (ucUcastSig << 8 ) | (ucBcastSig << 16); 
  sMessage.bodyval = uData;
  sMessage.type    = WLANTL_RX_FWD_CACHED;

  return vos_rx_mq_serialize(VOS_MQ_ID_TL, &sMessage);

} /* WLANTL_StartForwarding() */

/*===========================================================================

  FUNCTION    WLANTL_EnableCaching

  DESCRIPTION

    This function is used to enable caching only when assoc/reassoc req is send.
    that is cache packets only for such STA ID.


  DEPENDENCIES

    TL must have been initialized before this gets called.


  PARAMETERS

   staId:   station id

  RETURN VALUE

   none

============================================================================*/
void WLANTL_EnableCaching(v_U8_t staId)
{
  v_PVOID_t pvosGCtx= vos_get_global_context(VOS_MODULE_ID_TL,NULL);
  WLANTL_CbType* pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Invalid TL pointer from pvosGCtx on "
           "WLANTL_EnableCaching"));
    return;
  }
  pTLCb->atlSTAClients[staId]->enableCaching = 1;
}

/*===========================================================================

  FUNCTION    WLANTL_AssocFailed

  DESCRIPTION

    This function is used by PE to notify TL that cache needs to flushed' 
    when association is not successfully completed 

    Internally, TL post a message to TX_Thread to serialize the request to 
    keep lock-free mechanism.

   
  DEPENDENCIES

    TL must have been initialized before this gets called.

   
  PARAMETERS

   ucSTAId:   station id 

  RETURN VALUE

   none
   
  SIDE EFFECTS
   There may be race condition that PE call this API and send another association
   request immediately with same staId before TX_thread can process the message.

   To avoid this, we might need PE to wait for TX_thread process the message,
   but this is not currently implemented. 
   
============================================================================*/
void WLANTL_AssocFailed(v_U8_t staId)
{
  // flushing frames and forwarding frames uses the same message
  // the only difference is what happens when the message is processed
  // if the STA exist, the frames will be forwarded
  // and if it doesn't exist, the frames will be flushed
  // in this case we know it won't exist so the DPU index signature values don't matter
  MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_ASSOC_FAILED,
                                                staId, 0));

  if(!VOS_IS_STATUS_SUCCESS(WLANTL_StartForwarding(staId,0,0)))
  {
    VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
       " %s fails to start forwarding (staId %d)", __func__, staId);
  }
}

  /*===========================================================================

  FUNCTION  WLANTL_Finish_ULA

  DESCRIPTION
     This function is used by HDD to notify TL to finish Upper layer authentication
     incase the last EAPOL packet is pending in the TL queue. 
     To avoid the race condition between sme set key and the last EAPOL packet 
     the HDD module calls this function just before calling the sme_RoamSetKey.

  DEPENDENCIES

     TL must have been initialized before this gets called.

  PARAMETERS

   callbackRoutine:   HDD Callback function.
   callbackContext : HDD userdata context.

   RETURN VALUE

   VOS_STATUS_SUCCESS/VOS_STATUS_FAILURE

  SIDE EFFECTS

============================================================================*/

VOS_STATUS WLANTL_Finish_ULA( void (*callbackRoutine) (void *callbackContext),
                              void *callbackContext)
{
   return WDA_DS_FinishULA( callbackRoutine, callbackContext);
}


/*===========================================================================

  FUNCTION    WLANTL_RegisterSTAClient

  DESCRIPTION

    This function is used by HDD to register as a client for data services
    with TL. HDD will call this API for each new station that it adds,
    thus having the flexibility of registering different callback for each
    STA it services.

  DEPENDENCIES

    TL must have been initialized before this gets called.

    Restriction:
      Main thread will have higher priority that Tx and Rx threads thus
      guaranteeing that a station will be added before any data can be
      received for it. (This enables TL to be lock free)

  PARAMETERS

   pvosGCtx:        pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
   pfnStARx:        function pointer to the receive packet handler from HDD
   pfnSTATxComp:    function pointer to the transmit complete confirmation
                    handler from HDD
   pfnSTAFetchPkt:  function pointer to the packet retrieval routine in HDD
   wSTADescType:    STA Descriptor, contains information related to the
                    new added STA

  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_INVAL: Input parameters are invalid
    VOS_STATUS_E_FAULT: Station ID is outside array boundaries or pointer to
                        TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was already registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_RegisterSTAClient
(
  v_PVOID_t                 pvosGCtx,
  WLANTL_STARxCBType        pfnSTARx,
  WLANTL_TxCompCBType       pfnSTATxComp,
  WLANTL_STAFetchPktCBType  pfnSTAFetchPkt,
  WLAN_STADescType*         pwSTADescType,
  v_S7_t                    rssi
)
{
  WLANTL_CbType*  pTLCb = NULL;
  WLANTL_STAClientType* pClientSTA = NULL;
  v_U8_t    ucTid = 0;/*Local variable to clear previous replay counters of STA on all TIDs*/
  v_U32_t   istoggleArpEnb = 0;
  tpAniSirGlobal pMac;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  ENTER();
  if (( NULL == pwSTADescType ) || ( NULL == pfnSTARx ) ||
      ( NULL == pfnSTAFetchPkt ))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_RegisterSTAClient"));
    return VOS_STATUS_E_INVAL;
  }

  if ( WLANTL_STA_ID_INVALID( pwSTADescType->ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid station id requested on WLANTL_RegisterSTAClient"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_RegisterSTAClient"));
    return VOS_STATUS_E_FAULT;
  }

  //Code for checking and allocating memory for new STA
  if ( NULL == pTLCb->atlSTAClients[pwSTADescType->ucSTAId] ){
      pTLCb->atlSTAClients[pwSTADescType->ucSTAId] = vos_mem_malloc(sizeof(WLANTL_STAClientType));
      if ( NULL == pTLCb->atlSTAClients[pwSTADescType->ucSTAId] ){
          TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL: STA Client memory allocation failed in WLANTL_RegisterSTAClient"));
          return VOS_STATUS_E_FAILURE;
      }
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
          "WLAN TL: STA Client memory allocation in WLANTL_RegisterSTAClient"));
      vos_mem_zero((v_VOID_t *) pTLCb->atlSTAClients[pwSTADescType->ucSTAId],sizeof(WLANTL_STAClientType));
  }

  //Assigning the pointer to local variable for easy access in future
  pClientSTA = pTLCb->atlSTAClients[pwSTADescType->ucSTAId];
  if ( 0 != pClientSTA->ucExists )
  {
    pClientSTA->ucExists++;
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL:Station was already registered on WLANTL_RegisterSTAClient"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Register station with TL
   ------------------------------------------------------------------------*/
  MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_REGISTER_STA_CLIENT,
                   pwSTADescType->ucSTAId, (unsigned )
                              (*(pwSTADescType->vSTAMACAddress.bytes+2)<<24 |
                               *(pwSTADescType->vSTAMACAddress.bytes+3)<<16 |
                               *(pwSTADescType->vSTAMACAddress.bytes+4)<<8 |
                               *(pwSTADescType->vSTAMACAddress.bytes+5))));

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Registering STA Client ID: %d", pwSTADescType->ucSTAId ));

  pClientSTA->pfnSTARx       = pfnSTARx;
  pClientSTA->pfnSTAFetchPkt = pfnSTAFetchPkt;

  /* Only register if different from NULL - TL default Tx Comp Cb will
    release the vos packet */
  if ( NULL != pfnSTATxComp )
  {
    pClientSTA->pfnSTATxComp   = pfnSTATxComp;
  }

  pClientSTA->tlState  = WLANTL_STA_INIT;
  pClientSTA->tlPri    = WLANTL_STA_PRI_NORMAL;
  pClientSTA->wSTADesc.ucSTAId  = pwSTADescType->ucSTAId;
  pClientSTA->ptkInstalled = 0;

  pMac = vos_get_context(VOS_MODULE_ID_PE, pvosGCtx);
  if ( NULL != pMac )
  {
    wlan_cfgGetInt(pMac, WNI_CFG_TOGGLE_ARP_BDRATES, &istoggleArpEnb);
  }
  pClientSTA->arpRate = istoggleArpEnb ? ENABLE_ARP_TOGGLE : DISABLE_ARP_TOGGLE;
  pClientSTA->arpOnWQ5 = istoggleArpEnb == SEND_ARP_ON_WQ5;

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
   "WLAN TL:Registering STA Client ID: %d with UC %d and BC %d toggleArp :%hhu",
    pwSTADescType->ucSTAId, pwSTADescType->ucUcastSig,
    pwSTADescType->ucBcastSig, pClientSTA->arpRate));

  pClientSTA->wSTADesc.wSTAType = pwSTADescType->wSTAType;

  pClientSTA->wSTADesc.ucQosEnabled = pwSTADescType->ucQosEnabled;

  pClientSTA->wSTADesc.ucAddRmvLLC = pwSTADescType->ucAddRmvLLC;

  pClientSTA->wSTADesc.ucProtectedFrame = pwSTADescType->ucProtectedFrame;

#ifdef FEATURE_WLAN_ESE
  pClientSTA->wSTADesc.ucIsEseSta = pwSTADescType->ucIsEseSta;

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Registering STA Client ID: %d QoS %d Add LLC %d ProtFrame %d EseSta %d",
             pwSTADescType->ucSTAId, 
             pwSTADescType->ucQosEnabled,
             pwSTADescType->ucAddRmvLLC,
             pwSTADescType->ucProtectedFrame,
             pwSTADescType->ucIsEseSta));
#else

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Registering STA Client ID: %d QoS %d Add LLC %d ProtFrame %d", 
             pwSTADescType->ucSTAId, 
             pwSTADescType->ucQosEnabled,
             pwSTADescType->ucAddRmvLLC,
             pwSTADescType->ucProtectedFrame));

#endif //FEATURE_WLAN_ESE
#ifdef WLAN_SOFTAP_VSTA_FEATURE
  // if this station was not allocated resources to perform HW-based
  // TX frame translation then force SW-based TX frame translation
  // otherwise use the frame translation supplied by the client

  if (!WDA_IsHwFrameTxTranslationCapable(pvosGCtx, pwSTADescType->ucSTAId)
      || ( WLAN_STA_BT_AMP == pwSTADescType->wSTAType))
  {
      pwSTADescType->ucSwFrameTXXlation = 1;
  }
#endif

  pClientSTA->wSTADesc.ucSwFrameTXXlation = pwSTADescType->ucSwFrameTXXlation;
  pClientSTA->wSTADesc.ucSwFrameRXXlation = pwSTADescType->ucSwFrameRXXlation;

#ifdef FEATURE_WLAN_WAPI
  pClientSTA->wSTADesc.ucIsWapiSta = pwSTADescType->ucIsWapiSta;
#endif /* FEATURE_WLAN_WAPI */

  vos_copy_macaddr( &pClientSTA->wSTADesc.vSTAMACAddress, &pwSTADescType->vSTAMACAddress);

  vos_copy_macaddr( &pClientSTA->wSTADesc.vBSSIDforIBSS, &pwSTADescType->vBSSIDforIBSS);

  vos_copy_macaddr( &pClientSTA->wSTADesc.vSelfMACAddress, &pwSTADescType->vSelfMACAddress);

  /* In volans release L replay check is done at TL */
  pClientSTA->ucIsReplayCheckValid = pwSTADescType->ucIsReplayCheckValid;
  pClientSTA->ulTotalReplayPacketsDetected =  0;
/*Clear replay counters of the STA on all TIDs*/
  for(ucTid = 0; ucTid < WLANTL_MAX_TID ; ucTid++)
  {
    pClientSTA->ullReplayCounter[ucTid] =  0;
  }

  /*--------------------------------------------------------------------
      Set the AC for the registered station to the highest priority AC
      Even if this AC is not supported by the station, correction will be
      made in the main TL loop after the supported mask is properly
      updated in the pending packets call
    --------------------------------------------------------------------*/
  pClientSTA->ucCurrentAC     = WLANTL_AC_HIGH_PRIO;
  pClientSTA->ucCurrentWeight = 0;
  pClientSTA->ucServicedAC    = WLANTL_AC_BK;
  pClientSTA->ucEapolPktPending = 0;

  vos_mem_zero( pClientSTA->aucACMask, sizeof(pClientSTA->aucACMask));

  vos_mem_zero( &pClientSTA->wUAPSDInfo, sizeof(pClientSTA->wUAPSDInfo));

  /*--------------------------------------------------------------------
    Reordering info and AMSDU de-aggregation
    --------------------------------------------------------------------*/
  vos_mem_zero( pClientSTA->atlBAReorderInfo,
     sizeof(pClientSTA->atlBAReorderInfo[0])*
     WLAN_MAX_TID);

  vos_mem_zero( pClientSTA->aucMPDUHeader,
                WLANTL_MPDU_HEADER_LEN);

  pClientSTA->ucMPDUHeaderLen   = 0;
  pClientSTA->vosAMSDUChain     = NULL;
  pClientSTA->vosAMSDUChainRoot = NULL;


  /* Reorder LOCK
   * During handle normal RX frame within RX thread,
   * if MC thread try to preempt, ADDBA, DELBA, TIMER
   * Context should be protected from race */
  for (ucTid = 0; ucTid < WLAN_MAX_TID ; ucTid++)
  {
    if (!VOS_IS_STATUS_SUCCESS(
        vos_lock_init(&pClientSTA->atlBAReorderInfo[ucTid].reorderLock)))
    {
       TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "Lock Init Fail"));
       return VOS_STATUS_E_FAILURE;
    }
  }
  /*--------------------------------------------------------------------
    Stats info
    --------------------------------------------------------------------*/
  vos_mem_zero( pClientSTA->auRxCount,
      sizeof(pClientSTA->auRxCount[0])*
      WLAN_MAX_TID);

  vos_mem_zero( pClientSTA->auTxCount,
      sizeof(pClientSTA->auRxCount[0])*
      WLAN_MAX_TID);
  /* Initial RSSI is always reported as zero because TL doesnt have enough
     data to calculate RSSI. So to avoid reporting zero, we are initializing
     RSSI with RSSI saved in BssDescription during scanning. */
  pClientSTA->rssiAvg = rssi;
  pClientSTA->rssiAvgBmps = rssi;
#ifdef FEATURE_WLAN_TDLS
  if(WLAN_STA_TDLS == pClientSTA->wSTADesc.wSTAType)
  {
    /* If client is TDLS, use TDLS specific alpha */
    pClientSTA->rssiAlpha = WLANTL_HO_TDLS_ALPHA;
  }
  else
  {
    pClientSTA->rssiAlpha = WLANTL_HO_DEFAULT_ALPHA;
  }
#else
    pClientSTA->rssiAlpha = WLANTL_HO_DEFAULT_ALPHA;
#endif /* FEATURE_WLAN_TDLS */
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
  pClientSTA->rssiDataAlpha = WLANTL_HO_DEFAULT_ALPHA;
  pClientSTA->interfaceStats.accessCategoryStats[0].ac = WLANTL_AC_BK;
  pClientSTA->interfaceStats.accessCategoryStats[1].ac = WLANTL_AC_BE;
  pClientSTA->interfaceStats.accessCategoryStats[2].ac = WLANTL_AC_VI;
  pClientSTA->interfaceStats.accessCategoryStats[3].ac = WLANTL_AC_VO;
#endif

  /*Tx not suspended and station fully registered*/
  vos_atomic_set_U8(
        &pClientSTA->ucTxSuspended, 0);

  /* Used until multiple station support will be added*/
  pTLCb->ucRegisteredStaId = pwSTADescType->ucSTAId;

  /* Save the BAP station ID for future usage */
  if ( WLAN_STA_BT_AMP == pwSTADescType->wSTAType )
  {
    pTLCb->tlBAPClient.ucBAPSTAId = pwSTADescType->ucSTAId;
  }

  /*------------------------------------------------------------------------
    Statistics info 
    -----------------------------------------------------------------------*/
  memset(&pClientSTA->trafficStatistics,
         0, sizeof(WLANTL_TRANSFER_STA_TYPE));


  /*------------------------------------------------------------------------
    Start with the state suggested by client caller
    -----------------------------------------------------------------------*/
  pClientSTA->tlState = pwSTADescType->ucInitState;
  /*-----------------------------------------------------------------------
    After all the init is complete we can mark the existance flag 
    ----------------------------------------------------------------------*/
  pClientSTA->ucExists++;

  //flow control fields init
  pClientSTA->ucLwmModeEnabled = FALSE;
  pClientSTA->ucLwmEventReported = FALSE;
  pClientSTA->bmuMemConsumed = 0;
  pClientSTA->uIngress_length = 0;
  pClientSTA->uBuffThresholdMax = WLANTL_STA_BMU_THRESHOLD_MAX;

  pClientSTA->uLwmThreshold = WLANTL_STA_BMU_THRESHOLD_MAX / 3;

  //@@@ HDDSOFTAP does not queue unregistered packet for now
  if ( WLAN_STA_SOFTAP != pwSTADescType->wSTAType )
  { 
    /*------------------------------------------------------------------------
      Forward received frames while STA was not yet registered 
    -  ----------------------------------------------------------------------*/
    if(!VOS_IS_STATUS_SUCCESS(WLANTL_StartForwarding( pwSTADescType->ucSTAId, 
                              pwSTADescType->ucUcastSig, 
                              pwSTADescType->ucBcastSig)))
    {
      VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         " %s fails to start forwarding", __func__);
    }
#ifdef FEATURE_WLAN_TDLS
    if( WLAN_STA_TDLS == pwSTADescType->wSTAType )
        pTLCb->ucTdlsPeerCount++;
#endif
  }
  return VOS_STATUS_SUCCESS;
}/* WLANTL_RegisterSTAClient */

/*===========================================================================

  FUNCTION    WLANTL_ClearSTAClient

  DESCRIPTION

    HDD will call this API when it no longer needs data services for the
    particular station.

  DEPENDENCIES

    A station must have been registered before the clear registration is
    called.

  PARAMETERS

   pvosGCtx:        pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
   ucSTAId:         identifier for the STA to be cleared

  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Station ID is outside array boundaries or pointer to
                        TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_ClearSTAClient
(
  v_PVOID_t         pvosGCtx,
  v_U8_t            ucSTAId
)
{
  WLANTL_CbType*  pTLCb = NULL; 
  v_U8_t  ucIndex;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  ENTER();
  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Invalid station id requested on WLANTL_ClearSTAClient"));
    return VOS_STATUS_E_FAULT;
  }
  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
       "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ClearSTAClient"));
    return VOS_STATUS_E_FAULT;
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pTLCb->atlSTAClients[ucSTAId]->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL:Station was not previously registered on WLANTL_ClearSTAClient"));
    /* Clean packets cached for the STA */
    WLANTL_StartForwarding(ucSTAId,0,0);
    return VOS_STATUS_E_EXISTS;
  }

  /* Delete BA sessions on all TID's */
  for (ucIndex = 0; ucIndex < WLAN_MAX_TID ; ucIndex++)
  {
     WLANTL_BaSessionDel(pvosGCtx, ucSTAId, ucIndex);
     vos_lock_destroy(&pTLCb->atlSTAClients[ucSTAId]->atlBAReorderInfo[ucIndex].reorderLock);
  }

#ifdef FEATURE_WLAN_TDLS
  /* decrement ucTdlsPeerCount only if it is non-zero */  
  if(WLAN_STA_TDLS == pTLCb->atlSTAClients[ucSTAId]->wSTADesc.wSTAType
      && pTLCb->ucTdlsPeerCount)
      pTLCb->ucTdlsPeerCount--;
#endif

  /*------------------------------------------------------------------------
    Clear station
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Clearing STA Client ID: %d", ucSTAId ));
  WLANTL_CleanSTA(pTLCb->atlSTAClients[ucSTAId], 1 /*empty packets*/);

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Clearing STA Reset History RSSI and Region number"));
  pTLCb->hoSupport.currentHOState.historyRSSI = 0;
  pTLCb->hoSupport.currentHOState.regionNumber = 0;

  return VOS_STATUS_SUCCESS;
}/* WLANTL_ClearSTAClient */

/*===========================================================================

  FUNCTION    WLANTL_ChangeSTAState

  DESCRIPTION

    HDD will make this notification whenever a change occurs in the
    connectivity state of a particular STA.

  DEPENDENCIES

    A station must have been registered before the change state can be
    called.

    RESTRICTION: A station is being notified as authenticated before the
                 keys are installed in HW. This way if a frame is received
                 before the keys are installed DPU will drop that frame.

    Main thread has higher priority that Tx and Rx threads thus guaranteeing
    the following:
        - a station will be in assoc state in TL before TL receives any data
          for it

  PARAMETERS

   pvosGCtx:        pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
   ucSTAId:         identifier for the STA that is pending transmission
   tlSTAState:     the new state of the connection to the given station


  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer to
                         TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_ChangeSTAState
(
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  WLANTL_STAStateType   tlSTAState
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( tlSTAState >= WLANTL_STA_MAX_STATE )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid parameter sent on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_INVAL;
  }

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:Invalid station id requested on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_FAULT;
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pTLCb->atlSTAClients[ucSTAId]->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
     "WLAN TL:Station was not previously registered on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Change STA state
    No need to lock this operation, see restrictions above
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Changing state for STA Client ID: %d from %d to %d",
             ucSTAId, pTLCb->atlSTAClients[ucSTAId]->tlState, tlSTAState));

  MTRACE(vos_trace(VOS_MODULE_ID_TL,
                   TRACE_CODE_TL_STA_STATE, ucSTAId,tlSTAState ));

  pTLCb->atlSTAClients[ucSTAId]->tlState = tlSTAState;

  return VOS_STATUS_SUCCESS;
}/* WLANTL_ChangeSTAState */

/*===========================================================================

  FUNCTION    WLANTL_UpdateTdlsSTAClient

  DESCRIPTION

    HDD will call this API when ENABLE_LINK happens and  HDD want to
    register QoS or other params for TDLS peers.

  DEPENDENCIES

    A station must have been registered before the WMM/QOS registration is
    called.

  PARAMETERS

   pvosGCtx:        pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
   wSTADescType:    STA Descriptor, contains information related to the
                    new added STA

  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Station ID is outside array boundaries or pointer to
                        TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/

VOS_STATUS
WLANTL_UpdateTdlsSTAClient
(
 v_PVOID_t                 pvosGCtx,
 WLAN_STADescType*         pwSTADescType
)
{
  WLANTL_CbType* pTLCb = NULL;
  WLANTL_STAClientType* pClientSTA = NULL;
  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb || ( WLAN_MAX_STA_COUNT <= pwSTADescType->ucSTAId))
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_UpdateTdlsSTAClient"));
      return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[pwSTADescType->ucSTAId];
  if ((NULL == pClientSTA) || 0 == pClientSTA->ucExists)
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                    "WLAN TL:Station not exists"));
      return VOS_STATUS_E_FAILURE;
  }

  pClientSTA->wSTADesc.ucQosEnabled = pwSTADescType->ucQosEnabled;

  return VOS_STATUS_SUCCESS;

}

VOS_STATUS WLANTL_SetMonRxCbk(v_PVOID_t pvosGCtx, WLANTL_MonRxCBType pfnMonRx)
{
  WLANTL_CbType*  pTLCb = NULL ;
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_RegisterSTAClient"));
    return VOS_STATUS_E_FAULT;
  }
  pTLCb->pfnMonRx = pfnMonRx;
  return VOS_STATUS_SUCCESS;
}

void WLANTL_SetIsConversionReq(v_PVOID_t pvosGCtx, v_BOOL_t isConversionReq)
{
  WLANTL_CbType*  pTLCb = NULL ;
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_RegisterSTAClient"));
    return;
  }
  pTLCb->isConversionReq = isConversionReq;
  return;
}


/*===========================================================================

  FUNCTION    WLANTL_STAPtkInstalled

  DESCRIPTION

    HDD will make this notification whenever PTK is installed for the STA

  DEPENDENCIES

    A station must have been registered before the change state can be
    called.

  PARAMETERS

   pvosGCtx:        pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
   ucSTAId:         identifier for the STA for which Pairwise key is
                    installed

  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer to
                         TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_STAPtkInstalled
(
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:Invalid station id requested on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         FL("WLAN TL:Invalid TL pointer from pvosGCtx")));
    return VOS_STATUS_E_FAULT;
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          FL("WLAN TL:Client Memory was not allocated")));
      return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pTLCb->atlSTAClients[ucSTAId]->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
     FL("WLAN TL:Station was not previously registered")));
    return VOS_STATUS_E_EXISTS;
  }

  pTLCb->atlSTAClients[ucSTAId]->ptkInstalled = 1;

  return VOS_STATUS_SUCCESS;
}/* WLANTL_STAPtkInstalled */

/*===========================================================================

  FUNCTION    WLANTL_GetSTAState

  DESCRIPTION

    Returns connectivity state of a particular STA.

  DEPENDENCIES

    A station must have been registered before its state can be retrieved.


  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    ucSTAId:        identifier of the station

    OUT
    ptlSTAState:    the current state of the connection to the given station


  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer to
                         TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetSTAState
(
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  WLANTL_STAStateType   *ptlSTAState
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == ptlSTAState )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid parameter sent on WLANTL_GetSTAState"));
    return VOS_STATUS_E_INVAL;
  }

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:Invalid station id requested on WLANTL_GetSTAState"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_GetSTAState"));
    return VOS_STATUS_E_FAULT;
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Client Memory was not allocated on %s", __func__));
        return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pTLCb->atlSTAClients[ucSTAId]->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
     "WLAN TL:Station was not previously registered on WLANTL_GetSTAState"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Get STA state
   ------------------------------------------------------------------------*/
  *ptlSTAState = pTLCb->atlSTAClients[ucSTAId]->tlState;

  return VOS_STATUS_SUCCESS;
}/* WLANTL_GetSTAState */

/*==========================================================================
  FUNCTION   WLANTL_UpdateSTABssIdforIBSS

  DESCRIPTION
    HDD will call this API to update the BSSID for this Station.

  DEPENDENCIES
    The HDD Should registered the staID with TL before calling this function.

  PARAMETERS

    IN
    pvosGCtx:    Pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context
    IN
    ucSTAId       The Station ID for Bssid to be updated
    IN
    pBssid          BSSID to be updated

  RETURN VALUE
      The result code associated with performing the operation

      VOS_STATUS_E_INVAL:  Input parameters are invalid
      VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer to
                           TL cb is NULL ; access would cause a page fault
      VOS_STATUS_E_EXISTS: Station was not registered
      VOS_STATUS_SUCCESS:  Everything is good :)

    SIDE EFFECTS
============================================================================*/


VOS_STATUS
WLANTL_UpdateSTABssIdforIBSS
(
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  v_U8_t               *pBssid
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:Invalid station id requested %s", __func__));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid TL pointer from pvosGCtx %s", __func__));
    return VOS_STATUS_E_FAULT;
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Client Memory was not allocated on %s", __func__));
        return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pTLCb->atlSTAClients[ucSTAId]->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
     "WLAN TL:Station was not previously registered %s", __func__));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Update the IBSS BSSID
   ------------------------------------------------------------------------*/
  vos_mem_copy( &pTLCb->atlSTAClients[ucSTAId]->wSTADesc.vBSSIDforIBSS,
                                     pBssid, sizeof(v_MACADDR_t));

  return VOS_STATUS_SUCCESS;
}

/*===========================================================================

  FUNCTION    WLANTL_STAPktPending

  DESCRIPTION

    HDD will call this API when a packet is pending transmission in its
    queues.

  DEPENDENCIES

    A station must have been registered before the packet pending
    notification can be sent.

    RESTRICTION: TL will not count packets for pending notification.
                 HDD is expected to send the notification only when
                 non-empty event gets triggered. Worst case scenario
                 is that TL might end up making a call when Hdds
                 queues are actually empty.

  PARAMETERS

    pvosGCtx:    pointer to the global vos context; a handle to TL's
                 control block can be extracted from its context
    ucSTAId:     identifier for the STA that is pending transmission

  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer
                         to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_STAPktPending
(
  v_PVOID_t            pvosGCtx,
  v_U8_t               ucSTAId,
  WLANTL_ACEnumType    ucAc
)
{
  WLANTL_CbType*  pTLCb = NULL;
  WLANTL_STAClientType* pClientSTA = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
      "WLAN TL:Packet pending indication for STA: %d AC: %d", ucSTAId, ucAc);

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid station id requested on WLANTL_STAPktPending"));
    return VOS_STATUS_E_FAULT;
  }
  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_STAPktPending"));
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Client Memory was not allocated on %s", __func__));
        return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pClientSTA->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL:Station was not previously registered on WLANTL_STAPktPending"));
    return VOS_STATUS_E_EXISTS;
  }

  /*---------------------------------------------------------------------
    Temporary fix to enable TL to fetch packets when multiple peers join
    an IBSS. To fix CR177301. Needs to go away when the actual fix of
    going through all STA's in round robin fashion gets merged in from
    BT AMP branch.
    --------------------------------------------------------------------*/
  pTLCb->ucRegisteredStaId = ucSTAId;

  if( WLANTL_STA_CONNECTED == pClientSTA->tlState )
  { /* EAPOL_HI_PRIORITY : need to find out whether EAPOL is pending before
       WLANTL_FetchPacket()/WLANTL_TxConn() is called.
       change STA_AUTHENTICATED != tlState to CONNECTED == tlState
       to make sure TL is indeed waiting for EAPOL.
       Just in the case when STA got disconnected shortly after connectection */
    pClientSTA->ucEapolPktPending = 1;

    MTRACE(vos_trace(VOS_MODULE_ID_TL,
           TRACE_CODE_TL_EAPOL_PKT_PENDING, ucSTAId, ucAc));

    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
      "WLAN TL:Packet pending indication for STA: %d AC: %d State: %d", 
               ucSTAId, ucAc, pClientSTA->tlState);
  }

  /*-----------------------------------------------------------------------
    Enable this AC in the AC mask in order for TL to start servicing it
    Set packet pending flag 
    To avoid race condition, serialize the updation of AC and AC mask 
    through WLANTL_TX_STAID_AC_IND message.
  -----------------------------------------------------------------------*/

      pClientSTA->aucACMask[ucAc] = 1;

      vos_atomic_set_U8( &pClientSTA->ucPktPending, 1);

      /*------------------------------------------------------------------------
        Check if there are enough resources for transmission and tx is not
        suspended.
        ------------------------------------------------------------------------*/
       if (( pTLCb->uResCount >=  WDA_TLI_MIN_RES_DATA ) &&
          ( 0 == pTLCb->ucTxSuspended ))
      {

        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
              "Issuing Xmit start request to BAL"));
           WDA_DS_StartXmit(pvosGCtx);
      }
      else
      {
        /*---------------------------------------------------------------------
          No error code is sent because TL will resume tx autonomously if
          resources become available or tx gets resumed
          ---------------------------------------------------------------------*/
        VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
              "WLAN TL:Request to send but condition not met. Res: %d,Suspend: %d",
              pTLCb->uResCount, pTLCb->ucTxSuspended );
      }
  return VOS_STATUS_SUCCESS;
}/* WLANTL_STAPktPending */

/*==========================================================================

  FUNCTION    WLANTL_SetSTAPriority

  DESCRIPTION

    TL exposes this API to allow upper layers a rough control over the
    priority of transmission for a given station when supporting multiple
    connections.

  DEPENDENCIES

    A station must have been registered before the change in priority can be
    called.

  PARAMETERS

    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    ucSTAId:        identifier for the STA that has to change priority

  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer
                         to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_SetSTAPriority
(
  v_PVOID_t            pvosGCtx,
  v_U8_t               ucSTAId,
  WLANTL_STAPriorityType   tlSTAPri
)
{
  WLANTL_CbType*  pTLCb = NULL;
  WLANTL_STAClientType* pClientSTA = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Invalid station id requested on WLANTL_SetSTAPriority"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_SetSTAPriority"));
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Client Memory was not allocated on %s", __func__));
        return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pClientSTA->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL:Station was not previously registered on WLANTL_SetSTAPriority"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Re-analize if lock is needed when adding multiple stations
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Changing state for STA Pri ID: %d from %d to %d",
             ucSTAId, pClientSTA->tlPri, tlSTAPri));
  pClientSTA->tlPri = tlSTAPri;

  return VOS_STATUS_SUCCESS;
}/* WLANTL_SetSTAPriority */


/*----------------------------------------------------------------------------
    INTERACTION WITH BAP
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANTL_RegisterBAPClient

  DESCRIPTION
    Called by SME to register itself as client for non-data BT-AMP packets.

  DEPENDENCIES
    TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or SME's control block can be extracted from its context
    pfnTlBAPRxFrm:  pointer to the receive processing routine for non-data
                    BT-AMP packets
    pfnFlushOpCompleteCb:
                    pointer to the call back function, for the Flush operation
                    completion.


  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer
                         to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: BAL client was already registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_RegisterBAPClient
(
  v_PVOID_t              pvosGCtx,
  WLANTL_BAPRxCBType     pfnTlBAPRxFrm,
  WLANTL_FlushOpCompCBType  pfnFlushOpCompleteCb
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == pfnTlBAPRxFrm )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid parameter sent on WLANTL_RegisterBAPClient"));
    return VOS_STATUS_E_INVAL;
  }

  if ( NULL == pfnFlushOpCompleteCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "Invalid Flush Complete Cb parameter sent on WLANTL_RegisterBAPClient"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_RegisterBAPClient"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Make sure this is the first registration attempt
   ------------------------------------------------------------------------*/
  if ( 0 != pTLCb->tlBAPClient.ucExists )
  {
    pTLCb->tlBAPClient.ucExists++;
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:BAP client was already registered"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Register station with TL
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Registering BAP Client" ));

  pTLCb->tlBAPClient.ucExists++;

  if ( NULL != pfnTlBAPRxFrm ) 
  {
    pTLCb->tlBAPClient.pfnTlBAPRx             = pfnTlBAPRxFrm;
  }

  pTLCb->tlBAPClient.pfnFlushOpCompleteCb   = pfnFlushOpCompleteCb;

  pTLCb->tlBAPClient.vosPendingDataBuff     = NULL;

  return VOS_STATUS_SUCCESS;
}/* WLANTL_RegisterBAPClient */


/*==========================================================================

  FUNCTION    WLANTL_TxBAPFrm

  DESCRIPTION
    BAP calls this when it wants to send a frame to the module

  DEPENDENCIES
    BAP must be registered with TL before this function can be called.

    RESTRICTION: BAP CANNOT push any packets to TL until it did not receive
                 a tx complete from the previous packet, that means BAP
                 sends one packet, wait for tx complete and then
                 sends another one

                 If BAP sends another packet before TL manages to process the
                 previously sent packet call will end in failure

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or BAP's control block can be extracted from its context
    vosDataBuff:   pointer to the vOSS buffer containing the packet to be
                    transmitted
    pMetaInfo:      meta information about the packet
    pfnTlBAPTxComp: pointer to a transmit complete routine for notifying
                    the result of the operation over the bus

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a
                         page fault
    VOS_STATUS_E_EXISTS: BAL client was not yet registered
    VOS_STATUS_E_BUSY:   The previous BT-AMP packet was not yet transmitted
    VOS_STATUS_SUCCESS:  Everything is good :)

    Other failure messages may be returned from the BD header handling
    routines, please check apropriate API for more info.

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_TxBAPFrm
(
  v_PVOID_t               pvosGCtx,
  vos_pkt_t*              vosDataBuff,
  WLANTL_MetaInfoType*    pMetaInfo,
  WLANTL_TxCompCBType     pfnTlBAPTxComp
)
{
  WLANTL_CbType*  pTLCb      = NULL;
  VOS_STATUS      vosStatus  = VOS_STATUS_SUCCESS;
  v_MACADDR_t     vDestMacAddr;
  v_U16_t         usPktLen;
  v_U8_t          ucStaId = 0;
  v_U8_t          extraHeadSpace = 0;
  v_U8_t          ucWDSEnabled = 0;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_TxBAPFrm"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Ensure that BAP client was registered previously
   ------------------------------------------------------------------------*/
  if (( 0 == pTLCb->tlBAPClient.ucExists ) ||
      ( WLANTL_STA_ID_INVALID(pTLCb->tlBAPClient.ucBAPSTAId) ))
  {
    pTLCb->tlBAPClient.ucExists++;
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:BAP client not register on WLANTL_TxBAPFrm"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
   Check if any BT-AMP Frm is pending
  ------------------------------------------------------------------------*/
  if ( NULL != pTLCb->tlBAPClient.vosPendingDataBuff )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL:BT-AMP Frame already pending tx in TL on WLANTL_TxBAPFrm"));
    return VOS_STATUS_E_BUSY;
  }

  /*------------------------------------------------------------------------
    Save buffer and notify BAL; no lock is needed if the above restriction
    is met
    Save the tx complete fnct pointer as tl specific data in the vos buffer
   ------------------------------------------------------------------------*/

  /*------------------------------------------------------------------------
    Translate 802.3 frame to 802.11
   ------------------------------------------------------------------------*/
  ucStaId = pTLCb->tlBAPClient.ucBAPSTAId;
  if ( NULL == pTLCb->atlSTAClients[ucStaId] )
  {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Client Memory was not allocated on %s", __func__));
       return VOS_STATUS_E_FAILURE;
  }
  if (( 0 == pMetaInfo->ucDisableFrmXtl ) && 
      ( 0 != pTLCb->atlSTAClients[ucStaId]->wSTADesc.ucSwFrameTXXlation ))
  {
    vosStatus =  WLANTL_Translate8023To80211Header( vosDataBuff, &vosStatus,
                                                    pTLCb, &ucStaId,
                                                    pMetaInfo, &ucWDSEnabled,
                                                    &extraHeadSpace);

    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:Error when translating header WLANTL_TxBAPFrm"));

      return vosStatus;
    }

    pMetaInfo->ucDisableFrmXtl = 1;
  }

  /*-------------------------------------------------------------------------
    Call HAL to fill BD header
   -------------------------------------------------------------------------*/

  /* Adding Type, SubType which was missing for EAPOL from BAP */
  pMetaInfo->ucType |= (WLANTL_80211_DATA_TYPE << 4);
  pMetaInfo->ucType |= (WLANTL_80211_DATA_QOS_SUBTYPE);

  vosStatus = WDA_DS_BuildTxPacketInfo( pvosGCtx, vosDataBuff , 
                    &vDestMacAddr, pMetaInfo->ucDisableFrmXtl, 
                    &usPktLen, pTLCb->atlSTAClients[ucStaId]->wSTADesc.ucQosEnabled,
                    ucWDSEnabled, extraHeadSpace, pMetaInfo->ucType,
                            &pTLCb->atlSTAClients[ucStaId]->wSTADesc.vSelfMACAddress,
                    pMetaInfo->ucTID, 0 /* No ACK */, pMetaInfo->usTimeStamp,
                    pMetaInfo->ucIsEapol || pMetaInfo->ucIsWai, pMetaInfo->ucUP,
                    pMetaInfo->ucTxBdToken);

  if ( VOS_STATUS_SUCCESS != vosStatus )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Failed while building TX header %d", vosStatus));
    return vosStatus;
  }

  if ( NULL != pfnTlBAPTxComp )
  {
    vos_pkt_set_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_TL,
                               (v_PVOID_t)pfnTlBAPTxComp);
  }
  else
  {
    vos_pkt_set_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_TL,
                               (v_PVOID_t)WLANTL_TxCompDefaultCb);

  }

  vos_atomic_set( (uintptr_t*)&pTLCb->tlBAPClient.vosPendingDataBuff,
                      (uintptr_t)vosDataBuff);

  /*------------------------------------------------------------------------
    Check if thre are enough resources for transmission and tx is not
    suspended.
   ------------------------------------------------------------------------*/
  if (( pTLCb->uResCount >=  WDA_TLI_MIN_RES_BAP ) &&
      ( 0 == pTLCb->ucTxSuspended ))
  {
    WDA_DS_StartXmit(pvosGCtx);
  }
  else
  {
    /*---------------------------------------------------------------------
      No error code is sent because TL will resume tx autonomously if
      resources become available or tx gets resumed
     ---------------------------------------------------------------------*/
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
          "WLAN TL:Request to send from BAP but condition not met.Res: %d,"
                 "Suspend: %d", pTLCb->uResCount, pTLCb->ucTxSuspended ));
  }

  return VOS_STATUS_SUCCESS;
}/* WLANTL_TxBAPFrm */


/*----------------------------------------------------------------------------
    INTERACTION WITH SME
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANTL_GetRssi

  DESCRIPTION
    TL will extract the RSSI information from every data packet from the
    ongoing traffic and will store it. It will provide the result to SME
    upon request.

  DEPENDENCIES

    WARNING: the read and write of this value will not be protected
             by locks, therefore the information obtained after a read
             might not always be consistent.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or SME's control block can be extracted from its context
    ucSTAId:        station identifier for the requested value

    OUT
    puRssi:         the average value of the RSSI


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer
                         to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: STA was not yet registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetRssi
(
  v_PVOID_t        pvosGCtx,
  v_U8_t           ucSTAId,
  v_S7_t*          pRssi
)
{
  WLANTL_CbType*  pTLCb = NULL;
  WLANTL_STAClientType* pClientSTA = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == pRssi )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_GetRssi"));
    return VOS_STATUS_E_INVAL;
  }

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid station id requested on WLANTL_GetRssi"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_GetRssi"));
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Client Memory was not allocated on %s", __func__));
        return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pClientSTA->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Station was not previously registered on WLANTL_GetRssi"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Copy will not be locked; please read restriction
   ------------------------------------------------------------------------*/
  if(pTLCb->isBMPS || IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE)
  {
    *pRssi = pClientSTA->rssiAvgBmps;
    /* Check If RSSI is zero because we are reading rssAvgBmps updated by HAL in 
    previous GetStatsRequest. It may be updated as zero by Hal because EnterBmps 
    might not have happend by that time. Hence reading the most recent Rssi 
    calcluated by TL*/
    if(0 == *pRssi)
    {
      *pRssi = pClientSTA->rssiAvg;
    }
  }
  else
  {
    *pRssi = pClientSTA->rssiAvg;
  }

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                    "WLAN TL:WLANTL_GetRssi for STA: %d RSSI: %d%s",
                    ucSTAId, *pRssi,
                    pTLCb->isBMPS ? " in BMPS" : ""));

  return VOS_STATUS_SUCCESS;
}/* WLANTL_GetRssi */

/*==========================================================================

  FUNCTION    WLANTL_GetSnr

  DESCRIPTION
    TL will extract the SNR information from every data packet from the
    ongoing traffic and will store it. It will provide the result to SME
    upon request.

  DEPENDENCIES

    WARNING: the read and write of this value will not be protected
             by locks, therefore the information obtained after a read
             might not always be consistent.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or SME's control block can be extracted from its context
    ucSTAId:        station identifier for the requested value

    OUT
    pSnr:         the average value of the SNR


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer
                         to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: STA was not yet registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetSnr
(
  tANI_U8           ucSTAId,
  tANI_S8*          pSnr
)
{
  WLANTL_CbType*  pTLCb = NULL;
  WLANTL_STAClientType* pClientSTA = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if (NULL == pSnr)
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on %s", __func__));
    return VOS_STATUS_E_INVAL;
  }

  if (WLANTL_STA_ID_INVALID(ucSTAId))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid station id requested on %s", __func__));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(vos_get_global_context(VOS_MODULE_ID_TL, NULL));
  if (NULL == pTLCb)
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid TL pointer from pvosGCtx on %s", __func__));
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if (NULL == pClientSTA)
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
    return VOS_STATUS_E_FAILURE;
  }

  if (0 == pClientSTA->ucExists)
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Station was not previously registered on %s", __func__));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Copy will not be locked; please read restriction
   ------------------------------------------------------------------------*/
  if (pTLCb->isBMPS)
  {
    *pSnr = pClientSTA->snrAvgBmps;
  }
  else
  {
    /* SNR is averaged over WLANTL_MAX_SNR_DATA_SAMPLES, if there are not enough
     * data samples (snridx) to calculate the average then return the
     * average for the window of prevoius 20 packets. And if there aren't
     * enough samples and the average for previous window of 20 packets is
     * not available then return a predefined value
     *
     * NOTE: the SNR_HACK_BMPS value is defined to 127, documents from HW
     * team reveal that the SNR value has a ceiling well below 127 dBm,
     * so if SNR has value of 127 the userspace applications can know that
     * the SNR has not been computed yet because enough data was not
     * available for SNR calculation
     */
    if (pClientSTA->snrIdx > (WLANTL_MAX_SNR_DATA_SAMPLES/2)
        || !(pClientSTA->prevSnrAvg))
    {
       *pSnr = pClientSTA->snrSum / pClientSTA->snrIdx;
    }
    else if (pClientSTA->prevSnrAvg)
    {
       *pSnr = pClientSTA->prevSnrAvg;
    }
    else
    {
       *pSnr = SNR_HACK_BMPS;
    }
  }

  VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN TL:WLANTL_GetSnr for STA: %d SNR: %d%s",
            ucSTAId, *pSnr,
            pTLCb->isBMPS ? " in BMPS" : "");

  return VOS_STATUS_SUCCESS;
}/* WLANTL_GetSnr */
/*==========================================================================

  FUNCTION    WLANTL_GetLinkQuality

  DESCRIPTION
    TL will extract the SNR information from every data packet from the
    ongoing traffic and will store it. It will provide the result to SME
    upon request.

  DEPENDENCIES

    WARNING: the read and write of this value will not be protected
             by locks, therefore the information obtained after a read
             might not always be consistent.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or SME's control block can be extracted from its context
    ucSTAId:        station identifier for the requested value

    OUT
    puLinkQuality:         the average value of the SNR


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer
                         to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: STA was not yet registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetLinkQuality
(
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  v_U32_t*              puLinkQuality
)
{
  WLANTL_CbType*  pTLCb = NULL;
  WLANTL_STAClientType* pClientSTA = NULL;

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == puLinkQuality )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "Invalid parameter sent on WLANTL_GetLinkQuality"));
    return VOS_STATUS_E_INVAL;
  }

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "Invalid station id requested on WLANTL_GetLinkQuality"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "Invalid TL pointer from pvosGCtx on WLANTL_GetLinkQuality"));
    return VOS_STATUS_E_FAULT;
  }
  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Client Memory was not allocated on %s", __func__));
        return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pClientSTA->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "Station was not previously registered on WLANTL_GetLinkQuality"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Copy will not be locked; please read restriction
   ------------------------------------------------------------------------*/
  *puLinkQuality = pClientSTA->uLinkQualityAvg;

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLANTL_GetLinkQuality for STA: %d LinkQuality: %d", ucSTAId, *puLinkQuality));

  return VOS_STATUS_SUCCESS;
}/* WLANTL_GetLinkQuality */

/*==========================================================================

  FUNCTION    WLANTL_FlushStaTID

  DESCRIPTION
    TL provides this API as an interface to SME (BAP) layer. TL inturn posts a
    message to HAL. This API is called by the SME inorder to perform a flush
    operation.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or SME's control block can be extracted from its context
    ucSTAId:        station identifier for the requested value
    ucTid:          Tspec ID for the new BA session

    OUT
    The response for this post is received in the main thread, via a response
    message from HAL to TL.

  RETURN VALUE
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANTL_FlushStaTID
(
  v_PVOID_t             pvosGCtx,
  v_U8_t                ucSTAId,
  v_U8_t                ucTid
)
{
  WLANTL_CbType*  pTLCb = NULL;
  tpFlushACReq FlushACReqPtr = NULL;
  vos_msg_t vosMessage;


  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "Invalid station id requested on WLANTL_FlushStaTID"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "Invalid TL pointer from pvosGCtx on WLANTL_FlushStaTID"));
    return VOS_STATUS_E_FAULT;
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Client Memory was not allocated on %s", __func__));
        return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pTLCb->atlSTAClients[ucSTAId]->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "Station was not previously registered on WLANTL_FlushStaTID"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
  We need to post a message with the STA, TID value to HAL. HAL performs the flush
  ------------------------------------------------------------------------*/
  FlushACReqPtr = vos_mem_malloc(sizeof(tFlushACReq));

  if ( NULL == FlushACReqPtr )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL: fatal failure, cannot allocate Flush Req structure"));
    VOS_ASSERT(0);
    return VOS_STATUS_E_NOMEM;
  }

  // Start constructing the message for HAL
  FlushACReqPtr->mesgType    = SIR_TL_HAL_FLUSH_AC_REQ;
  FlushACReqPtr->mesgLen     = sizeof(tFlushACReq);
  FlushACReqPtr->mesgLen     = sizeof(tFlushACReq);
  FlushACReqPtr->ucSTAId = ucSTAId;
  FlushACReqPtr->ucTid = ucTid;

  vosMessage.type            = WDA_TL_FLUSH_AC_REQ;
  vosMessage.bodyptr = (void *)FlushACReqPtr;

  vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
  return VOS_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------
    INTERACTION WITH PE
 ---------------------------------------------------------------------------*/
/*==========================================================================

  FUNCTION    WLANTL_updateSpoofMacAddr

  DESCRIPTION
    Called by HDD to update macaddr

  DEPENDENCIES
    TL must be initialized before this API can be called.

  PARAMETERS

    IN
    pvosGCtx:           pointer to the global vos context; a handle to
                        TL's control block can be extracted from its context
    spoofMacAddr:     spoofed mac adderess
    selfMacAddr:        self Mac Address

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a
                         page fault
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_updateSpoofMacAddr
(
  v_PVOID_t               pvosGCtx,
  v_MACADDR_t*            spoofMacAddr,
  v_MACADDR_t*            selfMacAddr
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ChangeSTAState");
    return VOS_STATUS_E_FAULT;
  }

  vos_mem_copy(pTLCb->spoofMacAddr.selfMac.bytes, selfMacAddr,
                                                         VOS_MAC_ADDRESS_LEN);
  vos_mem_copy(pTLCb->spoofMacAddr.spoofMac.bytes, spoofMacAddr,
                                                         VOS_MAC_ADDRESS_LEN);

  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
                "TL: SelfSTA mac Addr for current Scan "MAC_ADDRESS_STR,
                        MAC_ADDR_ARRAY(pTLCb->spoofMacAddr.selfMac.bytes));

  return VOS_STATUS_SUCCESS;
}/* WLANTL_updateSpoofMacAddr */
/*==========================================================================

  FUNCTION    WLANTL_RegisterMgmtFrmClient

  DESCRIPTION
    Called by PE to register as a client for management frames delivery.

  DEPENDENCIES
    TL must be initialized before this API can be called.

  PARAMETERS

    IN
    pvosGCtx:           pointer to the global vos context; a handle to
                        TL's control block can be extracted from its context
    pfnTlMgmtFrmRx:     pointer to the receive processing routine for
                        management frames

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a
                         page fault
    VOS_STATUS_E_EXISTS: Mgmt Frame client was already registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_RegisterMgmtFrmClient
(
  v_PVOID_t               pvosGCtx,
  WLANTL_MgmtFrmRxCBType  pfnTlMgmtFrmRx
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == pfnTlMgmtFrmRx )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Invalid parameter sent on WLANTL_RegisterMgmtFrmClient"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Make sure this is the first registration attempt
   ------------------------------------------------------------------------*/
  if ( 0 != pTLCb->tlMgmtFrmClient.ucExists )
  {
    pTLCb->tlMgmtFrmClient.ucExists++;
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Management frame client was already registered"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Register station with TL
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN TL:Registering Management Frame Client" ));

  pTLCb->tlMgmtFrmClient.ucExists++;

  if ( NULL != pfnTlMgmtFrmRx )
  {
    pTLCb->tlMgmtFrmClient.pfnTlMgmtFrmRx = pfnTlMgmtFrmRx;
  }

  pTLCb->tlMgmtFrmClient.vosPendingDataBuff = NULL;

  return VOS_STATUS_SUCCESS;
}/* WLANTL_RegisterMgmtFrmClient */

/*==========================================================================

  FUNCTION    WLANTL_DeRegisterMgmtFrmClient

  DESCRIPTION
    Called by PE to deregister as a client for management frames delivery.

  DEPENDENCIES
    TL must be initialized before this API can be called.

  PARAMETERS

    IN
    pvosGCtx:           pointer to the global vos context; a handle to
                        TL's control block can be extracted from its context
  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a
                         page fault
    VOS_STATUS_E_EXISTS: Mgmt Frame client was never registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_DeRegisterMgmtFrmClient
(
  v_PVOID_t               pvosGCtx
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Make sure this is the first registration attempt
   ------------------------------------------------------------------------*/
  if ( 0 == pTLCb->tlMgmtFrmClient.ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Management frame client was never registered"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Clear registration with TL
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN TL:Deregistering Management Frame Client" ));

  pTLCb->tlMgmtFrmClient.pfnTlMgmtFrmRx = WLANTL_MgmtFrmRxDefaultCb;
  if ( NULL != pTLCb->tlMgmtFrmClient.vosPendingDataBuff)
  {
    TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
              "WLAN TL:Management cache buffer not empty on deregistering"
               " - dropping packet" ));
    vos_pkt_return_packet(pTLCb->tlMgmtFrmClient.vosPendingDataBuff);

    pTLCb->tlMgmtFrmClient.vosPendingDataBuff = NULL; 
  }

  pTLCb->tlMgmtFrmClient.ucExists = 0;

  return VOS_STATUS_SUCCESS;
}/* WLANTL_RegisterMgmtFrmClient */

/*==========================================================================

  FUNCTION    WLANTL_TxMgmtFrm

  DESCRIPTION
    Called by PE when it want to send out a management frame.
    HAL will also use this API for the few frames it sends out, they are not
    management frames howevere it is accepted that an exception will be
    allowed ONLY for the usage of HAL.
    Generic data frames SHOULD NOT travel through this function.

  DEPENDENCIES
    TL must be initialized before this API can be called.

    RESTRICTION: If PE sends another packet before TL manages to process the
                 previously sent packet call will end in failure

                 Frames comming through here must be 802.11 frames, frame
                 translation in UMA will be automatically disabled.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context;a handle to TL's
                    control block can be extracted from its context
    vosFrmBuf:      pointer to a vOSS buffer containing the management
                    frame to be transmitted
    usFrmLen:       the length of the frame to be transmitted; information
                    is already included in the vOSS buffer
    wFrmType:       the type of the frame being transmitted
    tid:            tid used to transmit this frame
    pfnCompTxFunc:  function pointer to the transmit complete routine
    pvBDHeader:     pointer to the BD header, if NULL it means it was not
                    yet constructed and it lies within TL's responsibility
                    to do so; if not NULL it is expected that it was
                    already packed inside the vos packet
    ucAckResponse:  flag notifying it an interrupt is needed for the
                    acknowledgement received when the frame is sent out
                    the air and ; the interrupt will be processed by HAL,
                    only one such frame can be pending in the system at
                    one time.


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a
                         page fault
    VOS_STATUS_E_EXISTS: Mgmt Frame client was not yet registered
    VOS_STATUS_E_BUSY:   The previous Mgmt packet was not yet transmitted
    VOS_STATUS_SUCCESS:  Everything is good :)

    Other failure messages may be returned from the BD header handling
    routines, please check apropriate API for more info.

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_TxMgmtFrm
(
  v_PVOID_t            pvosGCtx,
  vos_pkt_t*           vosFrmBuf,
  v_U16_t              usFrmLen,
  v_U8_t               wFrmType,
  v_U8_t               ucTid,
  WLANTL_TxCompCBType  pfnCompTxFunc,
  v_PVOID_t            pvBDHeader,
  v_U32_t              ucAckResponse,
  v_U32_t               ucTxBdToken
)
{
  WLANTL_CbType*  pTLCb = NULL;
  v_MACADDR_t     vDestMacAddr;
  VOS_STATUS      vosStatus = VOS_STATUS_SUCCESS;
  v_U16_t         usPktLen;
  v_U32_t         usTimeStamp = 0;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == vosFrmBuf )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_TxMgmtFrm"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGP(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
              "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_TxMgmtFrm"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Ensure that management frame client was previously registered
   ------------------------------------------------------------------------*/
  if ( 0 == pTLCb->tlMgmtFrmClient.ucExists )
  {
    pTLCb->tlMgmtFrmClient.ucExists++;
    TLLOGP(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
          "WLAN TL:Management Frame client not register on WLANTL_TxMgmtFrm"));
    return VOS_STATUS_E_EXISTS;
  }

   /*------------------------------------------------------------------------
    Check if any Mgmt Frm is pending
   ------------------------------------------------------------------------*/
  //vosTempBuff = pTLCb->tlMgmtFrmClient.vosPendingDataBuff;
  if ( NULL != pTLCb->tlMgmtFrmClient.vosPendingDataBuff )
  {

    TLLOGP(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
        "WLAN TL:Management Frame already pending tx in TL: failing old one"));


    /*Failing the tx for the previous packet enqued by PE*/
    //vos_atomic_set( (uintptr_t*)&pTLCb->tlMgmtFrmClient.vosPendingDataBuff,
    //                    (uintptr_t)NULL);

    //vos_pkt_get_user_data_ptr( vosTempBuff, VOS_PKT_USER_DATA_ID_TL,
    //                           (v_PVOID_t)&pfnTxComp);

    /*it should never be NULL - default handler should be registered if none*/
    //if ( NULL == pfnTxComp )
    //{
    //  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    //            "NULL pointer to Tx Complete on WLANTL_TxMgmtFrm");
    //  VOS_ASSERT(0);
    //  return VOS_STATUS_E_FAULT;
    //}

    //pfnTxComp( pvosGCtx, vosTempBuff, VOS_STATUS_E_RESOURCES );
    //return VOS_STATUS_E_BUSY;


    //pfnCompTxFunc( pvosGCtx, vosFrmBuf, VOS_STATUS_E_RESOURCES);
    return VOS_STATUS_E_RESOURCES;
  }


  /*------------------------------------------------------------------------
    Check if BD header was build, if not construct
   ------------------------------------------------------------------------*/
  if ( NULL == pvBDHeader )
  {
     v_MACADDR_t*     pvAddr2MacAddr;
     v_U8_t   uQosHdr = VOS_FALSE;

     /* Get address 2 of Mangement Frame to give to WLANHAL_FillTxBd */
     vosStatus = vos_pkt_peek_data( vosFrmBuf, 
                                    WLANTL_MAC_ADDR_ALIGN(1) + VOS_MAC_ADDR_SIZE,
                                    (v_PVOID_t)&pvAddr2MacAddr, VOS_MAC_ADDR_SIZE);

     if ( VOS_STATUS_SUCCESS != vosStatus )
     {
       TLLOGP(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
                "WLAN TL:Failed while attempting to get addr2 %d", vosStatus));
       return vosStatus;
     }

    /* ESE IAPP/TDLS Frame which are data frames but technically used
     * for management functionality comes through route.
     */
    if (WLANTL_IS_QOS_DATA_FRAME(wFrmType))                                      \
    {
        uQosHdr = VOS_TRUE;
    }

    if (WLANTL_IS_PROBE_REQ(wFrmType))
    {
        if (VOS_TRUE == vos_mem_compare((v_VOID_t*) pvAddr2MacAddr,
            (v_VOID_t*) &pTLCb->spoofMacAddr.spoofMac, VOS_MAC_ADDRESS_LEN))
        {
            pvAddr2MacAddr = (v_PVOID_t)pTLCb->spoofMacAddr.selfMac.bytes;
            VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                "TL: using self sta addr to get staidx for spoofed probe req "
                    MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pvAddr2MacAddr->bytes));
        }
    }

    /*----------------------------------------------------------------------
      Call WDA to build TX header
     ----------------------------------------------------------------------*/
    vosStatus = WDA_DS_BuildTxPacketInfo( pvosGCtx, vosFrmBuf , &vDestMacAddr, 
                   1 /* always 802.11 frames*/, &usPktLen, uQosHdr /*qos not enabled !!!*/, 
                   0 /* WDS off */, 0, wFrmType, pvAddr2MacAddr, ucTid,
                   ucAckResponse, usTimeStamp, 0, 0, ucTxBdToken);


    if ( !VOS_IS_STATUS_SUCCESS(vosStatus) )
    {
      TLLOGP(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
                "WLAN TL:Failed while attempting to build TX header %d", vosStatus));
      return vosStatus;
    }
   }/* if BD header not present */

  /*------------------------------------------------------------------------
    Save buffer and notify BAL; no lock is needed if the above restriction
    is met
    Save the tx complete fnct pointer as tl specific data in the vos buffer
   ------------------------------------------------------------------------*/
  if ( NULL != pfnCompTxFunc )
  {
    vos_pkt_set_user_data_ptr( vosFrmBuf, VOS_PKT_USER_DATA_ID_TL,
                               (v_PVOID_t)pfnCompTxFunc);
  }
  else
  {
    vos_pkt_set_user_data_ptr( vosFrmBuf, VOS_PKT_USER_DATA_ID_TL,
                               (v_PVOID_t)WLANTL_TxCompDefaultCb);

  }
  vos_atomic_set( (uintptr_t*)&pTLCb->tlMgmtFrmClient.vosPendingDataBuff,
                      (uintptr_t)vosFrmBuf);

  /*------------------------------------------------------------------------
    Check if thre are enough resources for transmission and tx is not
    suspended.
   ------------------------------------------------------------------------*/
  if ( pTLCb->uResCount >=  WDA_TLI_MIN_RES_MF )
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
              "WLAN TL:Issuing Xmit start request to BAL for MGMT"));
    vosStatus = WDA_DS_StartXmit(pvosGCtx);
    if(VOS_STATUS_SUCCESS != vosStatus)
    {
       TLLOGP(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
              "WLAN TL:WDA_DS_StartXmit fails. vosStatus %d", vosStatus));
       vos_atomic_set( (uintptr_t*)&pTLCb->tlMgmtFrmClient.vosPendingDataBuff,0);
    }
    return vosStatus;
    
  }
  else
  {
    /*---------------------------------------------------------------------
      No error code is sent because TL will resume tx autonomously if
      resources become available or tx gets resumed
     ---------------------------------------------------------------------*/
    TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
       "WLAN TL:Request to send for Mgmt Frm but condition not met. Res: %d",
               pTLCb->uResCount));
  }

  return VOS_STATUS_SUCCESS;
}/* WLANTL_TxMgmtFrm */

/*----------------------------------------------------------------------------
    INTERACTION WITH HAL
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANTL_ResetNotification

  DESCRIPTION
    HAL notifies TL when the module is being reset.
    Currently not used.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to TL cb is NULL ; access would cause a
                         page fault
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_ResetNotification
(
  v_PVOID_t   pvosGCtx
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ResetNotification"));
    return VOS_STATUS_E_FAULT;
  }

  WLANTL_CleanCB(pTLCb, 1 /*empty all queues and pending packets*/);
  return VOS_STATUS_SUCCESS;
}/* WLANTL_ResetNotification */

/*==========================================================================

  FUNCTION    WLANTL_SuspendDataTx

  DESCRIPTION
    HAL calls this API when it wishes to suspend transmission for a
    particular STA.

  DEPENDENCIES
    The STA for which the request is made must be first registered with
    TL by HDD.

    RESTRICTION:  In case of a suspend, the flag write and read will not be
                  locked: worst case scenario one more packet can get
                  through before the flag gets updated (we can make this
                  write atomic as well to guarantee consistency)

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    pucSTAId:       identifier of the station for which the request is made;
                    a value of NULL assumes suspend on all active station
    pfnSuspendTxCB: pointer to the suspend result notification in case the
                    call is asynchronous


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:   Station ID is outside array boundaries or pointer
                          to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS:  Station was not registered
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/

VOS_STATUS
WLANTL_SuspendDataTx
(
  v_PVOID_t              pvosGCtx,
  v_U8_t*                pucSTAId,
  WLANTL_SuspendCBType   pfnSuspendTx
)
{
  WLANTL_CbType*  pTLCb = NULL;
  vos_msg_t       vosMsg;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_SuspendDataTx"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Check the type of request: generic suspend, or per station suspend
   ------------------------------------------------------------------------*/
  if (NULL == pucSTAId)
  {
    /* General Suspend Request received */
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
              "WLAN TL:General suspend requested"));
    vos_atomic_set_U8( &pTLCb->ucTxSuspended, 1);
    vosMsg.reserved = WLAN_MAX_STA_COUNT;
  }
  else
  {
    if ( WLANTL_STA_ID_INVALID( *pucSTAId ) )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:Invalid station id %d requested on WLANTL_SuspendDataTx", *pucSTAId));
      return VOS_STATUS_E_FAULT;
    }

    if ( NULL == pTLCb->atlSTAClients[*pucSTAId] )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Invalid pTLCb->atlSTAClients pointer for STA Id :%d on "
            "WLANTL_SuspendDataTx", *pucSTAId));
      return VOS_STATUS_E_FAULT;
    }

    if ( 0 == pTLCb->atlSTAClients[*pucSTAId]->ucExists )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:Station %d was not previously registered on WLANTL_SuspendDataTx", *pucSTAId));
      return VOS_STATUS_E_EXISTS;
    }

    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
         "WLAN TL:Suspend request for station: %d", *pucSTAId));
    vos_atomic_set_U8( &pTLCb->atlSTAClients[*pucSTAId]->ucTxSuspended, 1);
    vosMsg.reserved = *pucSTAId;
  }

  /*------------------------------------------------------------------------
    Serialize request through TX thread
   ------------------------------------------------------------------------*/
  vosMsg.type     = WLANTL_TX_SIG_SUSPEND;
  vosMsg.bodyptr     = (v_PVOID_t)pfnSuspendTx;

  MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_SUSPEND_DATA_TX,
                    vosMsg.reserved , 0 ));

  if(!VOS_IS_STATUS_SUCCESS(vos_tx_mq_serialize( VOS_MQ_ID_TL, &vosMsg)))
  {
    VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
       " %s fails to post message", __func__);
  }

  return VOS_STATUS_SUCCESS;
}/* WLANTL_SuspendDataTx */

/*==========================================================================

  FUNCTION    WLANTL_ResumeDataTx

  DESCRIPTION
    Called by HAL to resume data transmission for a given STA.

    WARNING: If a station was individually suspended a global resume will
             not resume that station

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    pucSTAId:       identifier of the station which is being resumed; NULL
                    translates into global resume

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:   Station ID is outside array boundaries or pointer
                          to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS:  Station was not registered
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/

VOS_STATUS
WLANTL_ResumeDataTx
(
  v_PVOID_t      pvosGCtx,
  v_U8_t*        pucSTAId
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ResumeDataTx"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Check to see the type of resume
   ------------------------------------------------------------------------*/
  if ( NULL == pucSTAId )
  {
    MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_RESUME_DATA_TX,
                      41 , 0 ));

    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
              "WLAN TL:General resume requested"));
    vos_atomic_set_U8( &pTLCb->ucTxSuspended, 0);
  }
  else
  {
    MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_RESUME_DATA_TX,
                      *pucSTAId , 0 ));

    if ( WLANTL_STA_ID_INVALID( *pucSTAId ))
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Invalid station id %d requested on WLANTL_ResumeDataTx", *pucSTAId));
      return VOS_STATUS_E_FAULT;
    }

    if ( NULL == pTLCb->atlSTAClients[*pucSTAId] )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Invalid pTLCb->atlSTAClients pointer for STA Id :%d on "
            "WLANTL_ResumeDataTx", *pucSTAId));
      return VOS_STATUS_E_FAULT;
    }

    if ( 0 == pTLCb->atlSTAClients[*pucSTAId]->ucExists )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:Station %d was not previously registered on WLANTL_ResumeDataTx", *pucSTAId));
      return VOS_STATUS_E_EXISTS;
    }

    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
         "WLAN TL:Resume request for station: %d", *pucSTAId));
    vos_atomic_set_U8( &pTLCb->atlSTAClients[*pucSTAId]->ucTxSuspended, 0);
  }

  /*------------------------------------------------------------------------
    Resuming transmission
   ------------------------------------------------------------------------*/
  if (( pTLCb->uResCount >=  WDA_TLI_MIN_RES_MF ) &&
      ( 0 == pTLCb->ucTxSuspended ))
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:Resuming transmission"));
    return WDA_DS_StartXmit(pvosGCtx);
  }

  return VOS_STATUS_SUCCESS;
}/* WLANTL_ResumeDataTx */

/*==========================================================================
  FUNCTION    WLANTL_SuspendCB

  DESCRIPTION
    Callback function for serializing Suspend signal through Tx thread

  DEPENDENCIES
    Just notify HAL that suspend in TL is complete.

  PARAMETERS

   IN
   pvosGCtx:       pointer to the global vos context; a handle to TL's
                   control block can be extracted from its context
   pUserData:      user data sent with the callback

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)


  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_SuspendCB
(
  v_PVOID_t             pvosGCtx,
  WLANTL_SuspendCBType  pfnSuspendCB,
  v_U16_t               usReserved
)
{
  WLANTL_CbType*  pTLCb   = NULL;
  v_U8_t          ucSTAId = (v_U8_t)usReserved;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == pfnSuspendCB )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
               "WLAN TL: No Call back processing requested WLANTL_SuspendCB"));
    return VOS_STATUS_SUCCESS;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_SuspendCB"));
    return VOS_STATUS_E_FAULT;
  }

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    pfnSuspendCB(pvosGCtx, NULL, VOS_STATUS_SUCCESS);
  }
  else
  {
    pfnSuspendCB(pvosGCtx, &ucSTAId, VOS_STATUS_SUCCESS);
  }

  return VOS_STATUS_SUCCESS;
}/*WLANTL_SuspendCB*/


/*----------------------------------------------------------------------------
    CLIENT INDEPENDENT INTERFACE
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANTL_GetTxPktCount

  DESCRIPTION
    TL will provide the number of transmitted packets counted per
    STA per TID.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    ucSTAId:        identifier of the station
    ucTid:          identifier of the tspec

    OUT
    puTxPktCount:   the number of packets tx packet for this STA and TID

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input parameters are invalid
    VOS_STATUS_E_FAULT:   Station ID is outside array boundaries or pointer
                          to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS:  Station was not registered
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetTxPktCount
(
  v_PVOID_t      pvosGCtx,
  v_U8_t         ucSTAId,
  v_U8_t         ucTid,
  v_U32_t*       puTxPktCount
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == puTxPktCount )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid parameter sent on WLANTL_GetTxPktCount"));
    return VOS_STATUS_E_INVAL;
  }

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) || WLANTL_TID_INVALID( ucTid) )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL:Invalid station id %d/tid %d requested on WLANTL_GetTxPktCount",
            ucSTAId, ucTid));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check if station exists
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_GetTxPktCount"));
    return VOS_STATUS_E_FAULT;
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Client Memory was not allocated on %s", __func__));
        return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pTLCb->atlSTAClients[ucSTAId]->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL:Station was not previously registered on WLANTL_GetTxPktCount %d",
     ucSTAId));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Return data
   ------------------------------------------------------------------------*/
  //VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_MED,
    //         "WLAN TL:Requested tx packet count for STA: %d, TID: %d", 
      //         ucSTAId, ucTid);

  *puTxPktCount = pTLCb->atlSTAClients[ucSTAId]->auTxCount[ucTid];

  return VOS_STATUS_SUCCESS;
}/* WLANTL_GetTxPktCount */

/*==========================================================================

  FUNCTION    WLANTL_GetRxPktCount

  DESCRIPTION
    TL will provide the number of received packets counted per
    STA per TID.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    ucSTAId:        identifier of the station
    ucTid:          identifier of the tspec

   OUT
    puTxPktCount:   the number of packets rx packet for this STA and TID

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input parameters are invalid
    VOS_STATUS_E_FAULT:   Station ID is outside array boundaries or pointer
                          to TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS:  Station was not registered
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetRxPktCount
(
  v_PVOID_t      pvosGCtx,
  v_U8_t         ucSTAId,
  v_U8_t         ucTid,
  v_U32_t*       puRxPktCount
)
{
  WLANTL_CbType*  pTLCb = NULL;
  WLANTL_STAClientType* pClientSTA = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == puRxPktCount )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_GetRxPktCount"));
    return VOS_STATUS_E_INVAL;
  }

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) || WLANTL_TID_INVALID( ucTid) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL:Invalid station id %d/tid %d requested on WLANTL_GetRxPktCount",
             ucSTAId, ucTid));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_GetRxPktCount"));
    return VOS_STATUS_E_FAULT;
  }
  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Client Memory was not allocated on %s", __func__));
        return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pClientSTA->ucExists )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL:Station was not previously registered on WLANTL_GetRxPktCount"));
    return VOS_STATUS_E_EXISTS;
  }

  /*------------------------------------------------------------------------
    Return data
   ------------------------------------------------------------------------*/
  TLLOG3(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_MED,
            "WLAN TL:Requested rx packet count for STA: %d, TID: %d",
             ucSTAId, ucTid));

  *puRxPktCount = pClientSTA->auRxCount[ucTid];

  return VOS_STATUS_SUCCESS;
}/* WLANTL_GetRxPktCount */

VOS_STATUS
WLANTL_TxFCFrame
(
  v_PVOID_t       pvosGCtx
);

/*==========================================================================

  FUNCTION    WLANTL_IsEAPOLPending

  DESCRIPTION

    HDD calls this function when hdd_tx_timeout occurs. This checks whether
    EAPOL is pending.

  DEPENDENCIES

    HDD must have registered with TL at least one STA before this function
    can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context

  RETURN VALUE

    The result code associated with performing the operation

    Success : Indicates EAPOL frame is pending and sta is in connected state

    Failure : EAPOL frame is not pending

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANTL_IsEAPOLPending
(
  v_PVOID_t       pvosGCtx
)
{
   WLANTL_CbType*      pTLCb = NULL;
   v_U32_t             i = 0;
  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
    pTLCb = VOS_GET_TL_CB(pvosGCtx);
    if (NULL == pTLCb)
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Invalid TL pointer for pvosGCtx"));
      return VOS_STATUS_E_FAILURE;
    }
    /*---------------------------------------------------------------------
     Check to see if there was any EAPOL packet is pending
     *--------------------------------------------------------------------*/
    for ( i = 0; i < WLAN_MAX_STA_COUNT; i++)
    {
       if ((NULL != pTLCb->atlSTAClients[i]) &&
           (pTLCb->atlSTAClients[i]->ucExists) &&
           (0 == pTLCb->atlSTAClients[i]->ucTxSuspended) &&
           (WLANTL_STA_CONNECTED == pTLCb->atlSTAClients[i]->tlState) &&
           (pTLCb->atlSTAClients[i]->ucPktPending)
           )
           return VOS_STATUS_SUCCESS;
    }
    return VOS_STATUS_E_FAILURE;
}

/*============================================================================
                      TL INTERNAL API DEFINITION
============================================================================*/

/*==========================================================================

  FUNCTION    WLANTL_GetFrames

  DESCRIPTION

    BAL calls this function at the request of the lower bus interface.
    When this request is being received TL will retrieve packets from HDD
    in accordance with the priority rules and the count supplied by BAL.

  DEPENDENCIES

    HDD must have registered with TL at least one STA before this function
    can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or BAL's control block can be extracted from its context
    uSize:          maximum size accepted by the lower layer
    uFlowMask       TX flow control mask for Prima. Each bit is defined as 
                    WDA_TXFlowEnumType

    OUT
    vosDataBuff:   it will contain a pointer to the first buffer supplied
                    by TL, if there is more than one packet supplied, TL
                    will chain them through vOSS buffers

  RETURN VALUE

    The result code associated with performing the operation

    1 or more: number of required resources if there are still frames to fetch
    0 : error or HDD queues are drained

  SIDE EFFECTS

  NOTE
    
    Featurized uFlowMask. If we want to remove featurization, we need to change
    BAL on Volans.

============================================================================*/
v_U32_t
WLANTL_GetFrames
(
  v_PVOID_t       pvosGCtx,
  vos_pkt_t     **ppFrameDataBuff,
  v_U32_t         uSize,
  v_U8_t          uFlowMask,
  v_BOOL_t*       pbUrgent
)
{
   vos_pkt_t**         pvosDataBuff = (vos_pkt_t**)ppFrameDataBuff;
   WLANTL_CbType*      pTLCb = NULL;
   WLANTL_STAClientType* pClientSTA = NULL;
   v_U32_t             uRemaining = uSize;
   vos_pkt_t*          vosRoot;
   vos_pkt_t*          vosTempBuf;
   WLANTL_STAFuncType  pfnSTAFsm;
   v_U16_t             usPktLen;
   v_U32_t             uResLen;
   v_U8_t              ucSTAId;
   v_U8_t              ucAC;
   vos_pkt_t*          vosDataBuff;
   v_U32_t             uTotalPktLen;
   v_U32_t             i=0;
   v_U32_t             j=0;
   v_U32_t             ucResult = 0;
   VOS_STATUS          vosStatus;
   WLANTL_STAEventType   wSTAEvent;
   tBssSystemRole       systemRole;
   tpAniSirGlobal pMac;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if (( NULL == pTLCb ) || ( NULL == pvosDataBuff ))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ChangeSTAState"));
    return ucResult;
  }

  pMac = vos_get_context(VOS_MODULE_ID_PE, pvosGCtx);
  if ( NULL == pMac )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid pMac", __func__));
    return ucResult;
  }

  vosDataBuff = pTLCb->vosDummyBuf; /* Just to avoid checking for NULL at
                                         each iteration */

  pTLCb->uResCount = uSize;

  /*-----------------------------------------------------------------------
    Save the root as we will walk this chain as we fill it
   -----------------------------------------------------------------------*/
  vosRoot = vosDataBuff;
 
  /*-----------------------------------------------------------------------
    There is still data - until FSM function says otherwise
   -----------------------------------------------------------------------*/
  pTLCb->bUrgent      = FALSE;

  while (( pTLCb->tlConfigInfo.uMinFramesProcThres < pTLCb->uResCount ) &&
         ( 0 < uRemaining ))
  {
    systemRole = wdaGetGlobalSystemRole(pMac);
#ifdef WLAN_SOFTAP_FLOWCTRL_EN
/* FIXME: The code has been disabled since it is creating issues in power save */
    if (eSYSTEM_AP_ROLE == systemRole)
    {
       if (pTLCb->done_once == 0 && NULL == pTLCb->vosTxFCBuf)
       {
          WLANTL_TxFCFrame (pvosGCtx);
          pTLCb->done_once ++;
       }
    } 
    if ( NULL != pTLCb->vosTxFCBuf )
    {
       //there is flow control packet waiting to be sent
       WDA_TLI_PROCESS_FRAME_LEN( pTLCb->vosTxFCBuf, usPktLen, uResLen, uTotalPktLen);
    
       if ( ( pTLCb->uResCount > uResLen ) &&
            ( uRemaining > uTotalPktLen ) &&
            ( uFlowMask & ( 1 << WDA_TXFLOW_FC ) ) )
       {
          TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                   "WLAN TL:Chaining FC frame first on GetFrame"));

          vos_pkt_chain_packet( vosDataBuff, pTLCb->vosTxFCBuf, 1 /*true*/ );

          vos_atomic_set( (uintptr_t*)&pTLCb->vosTxFCBuf, (uintptr_t) NULL);

          /*FC frames cannot be delayed*/
          pTLCb->bUrgent      = TRUE;

          /*Update remaining len from SSC */
          uRemaining        -= (usPktLen + WDA_DXE_HEADER_SIZE);

          /*Update resource count */
          pTLCb->uResCount -= uResLen;
       }
       else
       {
          TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                "WLAN TL:send fc out of source %s", __func__));
          ucResult = ( pTLCb->uResCount > uResLen )?VOS_TRUE:VOS_FALSE;
          break; /* Out of resources or reached max len */
       }
   }
   else 
#endif //WLAN_SOFTAP_FLOWCTRL_EN

    if (( NULL != pTLCb->tlMgmtFrmClient.vosPendingDataBuff ) &&
        ( uFlowMask & ( 1 << WDA_TXFLOW_MGMT ) ) )
    {
      WDA_TLI_PROCESS_FRAME_LEN( pTLCb->tlMgmtFrmClient.vosPendingDataBuff,
                          usPktLen, uResLen, uTotalPktLen);

      if (usPktLen > WLANTL_MAX_ALLOWED_LEN)
      {
          usPktLen = WLANTL_MAX_ALLOWED_LEN;
          VOS_ASSERT(0);
      }

      if ( ( pTLCb->uResCount > uResLen ) &&
           ( uRemaining > uTotalPktLen ) &&
           ( uFlowMask & ( 1 << WDA_TXFLOW_MGMT ) ) )
      {
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                   "WLAN TL:Chaining management frame on GetFrame"));

        vos_pkt_chain_packet( vosDataBuff,
                              pTLCb->tlMgmtFrmClient.vosPendingDataBuff,
                              1 /*true*/ );

        vos_atomic_set( (uintptr_t*)&pTLCb->tlMgmtFrmClient.
                                  vosPendingDataBuff, (uintptr_t)NULL);

        /*management frames cannot be delayed*/
        pTLCb->bUrgent      = TRUE;

        /*Update remaining len from SSC */
        uRemaining        -= (usPktLen + WDA_DXE_HEADER_SIZE);

        /*Update resource count */
        pTLCb->uResCount -= uResLen;
      }
      else
      {
        ucResult = ( pTLCb->uResCount > uResLen )?VOS_TRUE:VOS_FALSE;
        break; /* Out of resources or reached max len */
      }
    }
    else if (( pTLCb->tlBAPClient.vosPendingDataBuff ) &&
             ( WDA_TLI_MIN_RES_BAP <= pTLCb->uResCount ) &&
             ( 0 == pTLCb->ucTxSuspended )          )
    {
      WDA_TLI_PROCESS_FRAME_LEN( pTLCb->tlBAPClient.vosPendingDataBuff,
                          usPktLen, uResLen, uTotalPktLen);

      if (usPktLen > WLANTL_MAX_ALLOWED_LEN)
      {
          usPktLen = WLANTL_MAX_ALLOWED_LEN;
          VOS_ASSERT(0);
      }

      if ( ( pTLCb->uResCount > (uResLen + WDA_TLI_MIN_RES_MF ) ) &&
           ( uRemaining > uTotalPktLen ))
      {
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                   "WLAN TL:Chaining BT-AMP frame on GetFrame"));

        vos_pkt_chain_packet( vosDataBuff,
                              pTLCb->tlBAPClient.vosPendingDataBuff,
                              1 /*true*/ );

        /*BAP frames cannot be delayed*/
        pTLCb->bUrgent      = TRUE;

        vos_atomic_set( (uintptr_t*)&pTLCb->tlBAPClient.vosPendingDataBuff,
                        (uintptr_t) NULL);

        /*Update remaining len from SSC */
        uRemaining        -=  (usPktLen + WDA_DXE_HEADER_SIZE);

        /*Update resource count */
        pTLCb->uResCount  -= uResLen;
      }
      else
      {
        ucResult = uResLen + WDA_TLI_MIN_RES_MF;
        break; /* Out of resources or reached max len */
      }
    }
    /* note: this feature implemented only after WLAN_INGETRATED_SOC */
    /* search 'EAPOL_HI_PRIORITY' will show EAPOL HI_PRIORITY change in TL and WDI
       by default, EAPOL will be treated as higher priority, which means
       use mgmt_pool and DXE_TX_HI prority channel.
       this is introduced to address EAPOL failure under high background traffic
       with multi-channel concurrent mode. But this change works in SCC or standalone, too.
       see CR#387009 and WCNSOS-8
     */
    else if (( WDA_TLI_MIN_RES_MF <= pTLCb->uResCount )&&
             ( 0 == pTLCb->ucTxSuspended ) &&
             ( uFlowMask & ( 1 << WDA_TXFLOW_MGMT ) )
            )
    {
        vosTempBuf = NULL;
        /*---------------------------------------------------------------------
         Check to see if there was any EAPOL packet is pending
         *--------------------------------------------------------------------*/
        for ( i = 0; i < WLAN_MAX_STA_COUNT; i++)
        {
           if ((NULL != pTLCb->atlSTAClients[i]) &&
               (pTLCb->atlSTAClients[i]->ucExists) &&
               (0 == pTLCb->atlSTAClients[i]->ucTxSuspended) &&
               (WLANTL_STA_CONNECTED == pTLCb->atlSTAClients[i]->tlState) &&
               (pTLCb->atlSTAClients[i]->ucPktPending)
               )
               break;
        }

        if (i >= WLAN_MAX_STA_COUNT)
        {
           /* No More to Serve Exit Get Frames */
           break;
        }
        /* Serve EAPOL frame with HI_FLOW_MASK */
        ucSTAId = i;

        pClientSTA = pTLCb->atlSTAClients[ucSTAId];

        MTRACE(vos_trace(VOS_MODULE_ID_TL,
               TRACE_CODE_TL_GET_FRAMES_EAPOL, ucSTAId, pClientSTA->tlState));

        if (pClientSTA->wSTADesc.wSTAType == WLAN_STA_INFRA)
        {
            if(0 != pClientSTA->aucACMask[WLANTL_AC_HIGH_PRIO])
            {
              pClientSTA->ucCurrentAC = WLANTL_AC_HIGH_PRIO;
              pTLCb->uCurServedAC = WLANTL_AC_HIGH_PRIO;
            }
            else
                break;
        }
        else
        {
            for (j = WLANTL_MAX_AC ; j > 0; j--)
            {
              if (0 != pClientSTA->aucACMask[j-1])
              {
                pClientSTA->ucCurrentAC = j-1;
                pTLCb->uCurServedAC = j-1;
                break;
              }
            }
        }

        wSTAEvent = WLANTL_TX_EVENT;

        pfnSTAFsm = tlSTAFsm[pClientSTA->tlState].
                        pfnSTATbl[wSTAEvent];

        if ( NULL != pfnSTAFsm )
        {
          pClientSTA->ucNoMoreData = 0;
          vosStatus  = pfnSTAFsm( pvosGCtx, ucSTAId, &vosTempBuf, VOS_FALSE);

          if (( VOS_STATUS_SUCCESS != vosStatus ) &&
              ( NULL != vosTempBuf ))
          {
               pClientSTA->pfnSTATxComp( pvosGCtx, vosTempBuf, vosStatus );
               vosTempBuf = NULL;
               break;
          }/* status success*/
        }

        if (NULL != vosTempBuf)
        {
            WDA_TLI_PROCESS_FRAME_LEN( vosTempBuf, usPktLen, uResLen, uTotalPktLen);

            if (usPktLen > WLANTL_MAX_ALLOWED_LEN)
            {
                usPktLen = WLANTL_MAX_ALLOWED_LEN;
                VOS_ASSERT(0);
            }

            TLLOG4(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
                      "WLAN TL:Resources needed by frame: %d", uResLen));

            if ( ( pTLCb->uResCount >= (uResLen + WDA_TLI_MIN_RES_MF ) ) &&
               ( uRemaining > uTotalPktLen )
               )
            {
              TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                        "WLAN TL:Chaining data frame on GetFrame"));

              vos_pkt_chain_packet( vosDataBuff, vosTempBuf, 1 /*true*/ );

              /*EAPOL frame cannot be delayed*/
              pTLCb->bUrgent      = TRUE;

              vosTempBuf =  NULL;

              /*Update remaining len from SSC */
              uRemaining  -= (usPktLen + WDA_DXE_HEADER_SIZE);

               /*Update resource count */
              pTLCb->uResCount  -= uResLen;

              //fow control update
              pClientSTA->uIngress_length += uResLen;
              pClientSTA->uBuffThresholdMax = (pClientSTA->uBuffThresholdMax >= uResLen) ?
                (pClientSTA->uBuffThresholdMax - uResLen) : 0;
              pClientSTA->ucEapolPktPending = 0;
              VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                "WLAN TL:GetFrames STA: %d EAPOLPktPending %d",
                         ucSTAId, pClientSTA->ucEapolPktPending);
            }
         }
         else
         {  // no EAPOL frames exit Get frames
            TLLOG2(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                   "WLAN TL:GetFrames STA: %d, no EAPOL frame, continue.",
                   ucSTAId));
            continue;
         }
    }

    else if (( WDA_TLI_MIN_RES_DATA <= pTLCb->uResCount ) &&
             ( 0 == pTLCb->ucTxSuspended ) &&
             ( uFlowMask & WLANTL_DATA_FLOW_MASK))
    {
      /*---------------------------------------------------------------------
        Check to see if there was any packet left behind previously due to
        size constraints
       ---------------------------------------------------------------------*/
      vosTempBuf = NULL;

      if ( NULL != pTLCb->vosTempBuf ) 
      {
        vosTempBuf          = pTLCb->vosTempBuf;
        pTLCb->vosTempBuf   = NULL;
        ucSTAId             = pTLCb->ucCachedSTAId; 
        ucAC                = pTLCb->ucCachedAC;

        if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
        {
            TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:Client Memory was not allocated on %s", __func__));
            continue;
        }

        pTLCb->atlSTAClients[ucSTAId]->ucNoMoreData = 0;
        pClientSTA = pTLCb->atlSTAClients[ucSTAId];

        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                   "WLAN TL:Chaining cached data frame on GetFrame"));
      }
      else
      {
        WLAN_TLGetNextTxIds( pvosGCtx, &ucSTAId);
        if (ucSTAId >= WLAN_MAX_STA_COUNT)
        {
         /* Packets start coming in even after insmod Without *
            starting Hostapd or Interface being up            *
            During which cases STAID is invaled and hence 
            the check. HalMsg_ScnaComplete Triggers */

            break;
        }
        /* ucCurrentAC should have correct AC to be served by calling
           WLAN_TLGetNextTxIds */
        pClientSTA = pTLCb->atlSTAClients[ucSTAId];
        if ( NULL == pClientSTA )
        {
            TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:Client Memory was not allocated on %s", __func__));
            continue;
        }

        ucAC = pClientSTA->ucCurrentAC;

        pClientSTA->ucNoMoreData = 1;
        TLLOG4(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
                   "WLAN TL: %s get one data frame, station ID %d ", __func__, ucSTAId));
        /*-------------------------------------------------------------------
        Check to see that STA is valid and tx is not suspended
         -------------------------------------------------------------------*/
        if ( ( ! WLANTL_STA_ID_INVALID( ucSTAId ) ) &&
           ( 0 == pClientSTA->ucTxSuspended ) &&
           ( 0 == pClientSTA->fcStaTxDisabled) )
        {
          TLLOG4(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
                   "WLAN TL: %s sta id valid and not suspended ",__func__));
          wSTAEvent = WLANTL_TX_EVENT;

          pfnSTAFsm = tlSTAFsm[pClientSTA->tlState].
                        pfnSTATbl[wSTAEvent];

          if ( NULL != pfnSTAFsm )
          {
            pClientSTA->ucNoMoreData = 0;
            vosStatus  = pfnSTAFsm( pvosGCtx, ucSTAId, &vosTempBuf, VOS_FALSE);

            if (( VOS_STATUS_SUCCESS != vosStatus ) &&
                ( NULL != vosTempBuf ))
            {
                 pClientSTA->pfnSTATxComp( pvosGCtx,
                                                             vosTempBuf,
                                                             vosStatus );
                 vosTempBuf = NULL;
            }/* status success*/
          }/*NULL function state*/
        }/* valid STA id and ! suspended*/
        else
        {
           if ( ! WLANTL_STA_ID_INVALID( ucSTAId ) ) 
           {
                TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                  "WLAN TL:Not fetching frame because suspended for sta ID %d", 
                   ucSTAId));
           }
        }
      }/* data */

      if ( NULL != vosTempBuf )
      {
        WDA_TLI_PROCESS_FRAME_LEN( vosTempBuf, usPktLen, uResLen, uTotalPktLen);

        if (usPktLen > WLANTL_MAX_ALLOWED_LEN)
        {
            usPktLen = WLANTL_MAX_ALLOWED_LEN;
            VOS_ASSERT(0);
        }

        TLLOG4(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
                  "WLAN TL:Resources needed by frame: %d", uResLen));

        if ( ( pTLCb->uResCount >= (uResLen + WDA_TLI_MIN_RES_BAP ) ) &&
             ( uRemaining > uTotalPktLen ) &&
             ( uFlowMask & WLANTL_DATA_FLOW_MASK ) )
        {
          TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                    "WLAN TL:Chaining data frame on GetFrame"));

          vos_pkt_chain_packet( vosDataBuff, vosTempBuf, 1 /*true*/ );
          vosTempBuf =  NULL;

          /*Update remaining len from SSC */
          uRemaining        -= (usPktLen + WDA_DXE_HEADER_SIZE);

           /*Update resource count */
          pTLCb->uResCount  -= uResLen;

          //fow control update
          pClientSTA->uIngress_length += uResLen;
          pClientSTA->uBuffThresholdMax = (pClientSTA->uBuffThresholdMax >= uResLen) ?
            (pClientSTA->uBuffThresholdMax - uResLen) : 0;

        }
        else
        {
          /* Store this for later tx - already fetched from HDD */
          pTLCb->vosTempBuf = vosTempBuf;
          pTLCb->ucCachedSTAId = ucSTAId;
          pTLCb->ucCachedAC    = ucAC;
          ucResult = uResLen + WDA_TLI_MIN_RES_BAP;
          TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "min %d res required by TL.", ucResult ));
          break; /* Out of resources or reached max len */
        }
      }
      else
      {
           for ( i = 0; i < WLAN_MAX_STA_COUNT; i++)
           {
              if (NULL != pTLCb->atlSTAClients[i] && (pTLCb->atlSTAClients[i]->ucExists) &&
                  (pTLCb->atlSTAClients[i]->ucPktPending))
              {
                  /* There is station to be Served */
                  break;
              }
           }
           if (i >= WLAN_MAX_STA_COUNT)
           {
              /* No More to Serve Exit Get Frames */
              break;
           }
           else
           {
              /* More to be Served */
              continue;
           }
        } 
      }
    else
    {
      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Returning from GetFrame: resources = %d suspended = %d",
                 pTLCb->uResCount, pTLCb->ucTxSuspended));
      /* TL is starving even when DXE is not in low resource condition 
         Return min resource number required and Let DXE deceide what to do */
      if(( 0 == pTLCb->ucTxSuspended ) && 
         ( uFlowMask & WLANTL_DATA_FLOW_MASK ) )
      {
         TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
             "WLAN TL:Returning from GetFrame: resources = %d",
                 pTLCb->uResCount));
         ucResult = WDA_TLI_MIN_RES_DATA;
      }
       break; /*out of min data resources*/
    }

    pTLCb->usPendingTxCompleteCount++;
    /* Move data buffer up one packet */
    vos_pkt_walk_packet_chain( vosDataBuff, &vosDataBuff, 0/*false*/ );
  }

  /*----------------------------------------------------------------------
    Packet chain starts at root + 1
   ----------------------------------------------------------------------*/
  vos_pkt_walk_packet_chain( vosRoot, &vosDataBuff, 1/*true*/ );

  *pvosDataBuff = vosDataBuff;
  if (pbUrgent)
  {
      *pbUrgent     = pTLCb->bUrgent;
  }
  else
  {
      VOS_ASSERT( pbUrgent );
  }
  return ucResult;
}/* WLANTL_GetFrames */


/*==========================================================================

  FUNCTION    WLANTL_TxComp

  DESCRIPTION
    It is being called by BAL upon asynchronous notification of the packet
    or packets  being sent over the bus.

  DEPENDENCIES
    Tx complete cannot be called without a previous transmit.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or BAL's control block can be extracted from its context
    vosDataBuff:   it will contain a pointer to the first buffer for which
                    the BAL report is being made, if there is more then one
                    packet they will be chained using vOSS buffers.
    wTxStatus:      the status of the transmitted packet, see above chapter
                    on HDD interaction for a list of possible values

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input parameters are invalid
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_E_EXISTS:  Station was not registered
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_TxComp
(
  v_PVOID_t       pvosGCtx,
  vos_pkt_t      *pFrameDataBuff,
  VOS_STATUS      wTxStatus
)
{
  vos_pkt_t*           vosDataBuff = (vos_pkt_t*)pFrameDataBuff;
  WLANTL_CbType*       pTLCb     = NULL;
  WLANTL_TxCompCBType  pfnTxComp = NULL;
  VOS_STATUS           vosStatus = VOS_STATUS_SUCCESS;
  vos_pkt_t*           vosTempTx = NULL;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == vosDataBuff )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Extraneous NULL data pointer on WLANTL_TxComp"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
   Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_TxComp"));
    return VOS_STATUS_E_FAULT;
  }

  while ((0 < pTLCb->usPendingTxCompleteCount) &&
         ( VOS_STATUS_SUCCESS == vosStatus ) &&
         ( NULL !=  vosDataBuff))
  {
    vos_pkt_get_user_data_ptr(  vosDataBuff, VOS_PKT_USER_DATA_ID_TL,
                               (v_PVOID_t)&pfnTxComp);

    /*it should never be NULL - default handler should be registered if none*/
    if ( NULL == pfnTxComp )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:NULL pointer to Tx Complete on WLANTL_TxComp"));
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAULT;
    }

    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:Calling Tx complete for pkt %p in function %p",
               vosDataBuff, pfnTxComp));

    vosTempTx = vosDataBuff;
    vosStatus = vos_pkt_walk_packet_chain( vosDataBuff,
                                           &vosDataBuff, 1/*true*/);

    pfnTxComp( pvosGCtx, vosTempTx, wTxStatus );

    pTLCb->usPendingTxCompleteCount--;
  }

 
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN TL: current TL values are: resources = %d "
            "pTLCb->usPendingTxCompleteCount = %d",
              pTLCb->uResCount, pTLCb->usPendingTxCompleteCount));

  return VOS_STATUS_SUCCESS;
}/* WLANTL_TxComp */

/*==========================================================================

  FUNCTION    WLANTL_CacheSTAFrame

  DESCRIPTION
    Internal utility function for for caching incoming data frames that do 
    not have a registered station yet. 

  DEPENDENCIES
    TL must be initiailized before this function gets called.
    In order to benefit from thsi caching, the components must ensure that
    they will only register with TL at the moment when they are fully setup
    and ready to receive incoming data 
   
  PARAMETERS

    IN
    
    pTLCb:                  TL control block
    ucSTAId:                station id
    vosTempBuff:            the data packet
    uDPUSig:                DPU signature of the incoming packet
    bBcast:                 true if packet had the MC/BC bit set 

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL or STA Id invalid ; access
                          would cause a page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
static VOS_STATUS
WLANTL_CacheSTAFrame
(
  WLANTL_CbType*    pTLCb,
  v_U8_t            ucSTAId,
  vos_pkt_t*        vosTempBuff,
  v_U32_t           uDPUSig,
  v_U8_t            bBcast,
  v_U8_t            ucFrmType
)
{
  v_U8_t    ucUcastSig;
  v_U8_t    ucBcastSig;
  v_BOOL_t  bOldSTAPkt;
  WLANTL_STAClientType* pClientSTA = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------  
     Sanity check 
   -------------------------------------------------------------------------*/ 
  if (( NULL == pTLCb ) || ( NULL == vosTempBuff ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL: Invalid input pointer on WLANTL_CacheSTAFrame TL %p"
               " Packet %p", pTLCb, vosTempBuff ));
    return VOS_STATUS_E_FAULT;
  }

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid station id requested on WLANTL_CacheSTAFrame"));
    return VOS_STATUS_E_FAULT;
  }
  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Attempting to cache pkt for STA %d, BD DPU Sig: %d with sig UC: %d, BC: %d", 
             ucSTAId, uDPUSig,
             pClientSTA->wSTADesc.ucUcastSig,
             pClientSTA->wSTADesc.ucBcastSig));

  if(WLANTL_IS_CTRL_FRAME(ucFrmType))
  {
      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLAN TL: No need to cache CTRL frame. Dropping"));
      vos_pkt_return_packet(vosTempBuff); 
      return VOS_STATUS_SUCCESS;
  }

  /*-------------------------------------------------------------------------
    Check if the packet that we are trying to cache belongs to the old
    registered station (if any) or the new (potentially)upcoming station
    
    - If the STA with this Id was never registered with TL - the signature
    will be invalid;
    - If the STA was previously registered TL will have cached the former
    set of DPU signatures
  -------------------------------------------------------------------------*/
  if ( bBcast )
  {
    ucBcastSig = (v_U8_t)uDPUSig;
    bOldSTAPkt = (( WLAN_TL_INVALID_B_SIG != 
                  pClientSTA->wSTADesc.ucBcastSig ) &&
      ( ucBcastSig == pClientSTA->wSTADesc.ucBcastSig ));
  }
  else
  {
    ucUcastSig = (v_U8_t)uDPUSig;
    bOldSTAPkt = (( WLAN_TL_INVALID_U_SIG != 
                    pClientSTA->wSTADesc.ucUcastSig ) &&
        ( ucUcastSig == pClientSTA->wSTADesc.ucUcastSig ));
  }

  /*------------------------------------------------------------------------
    If the value of the DPU SIG matches the old, this packet will not
    be cached as it belonged to the former association
    In case the SIG does not match - this is a packet for a potentially new
    associated station 
  -------------------------------------------------------------------------*/
  if ( bOldSTAPkt || bBcast )
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:Data packet matches old sig for sig DPU: %d UC: %d, "
               "BC: %d - dropping", 
               uDPUSig, 
               pClientSTA->wSTADesc.ucUcastSig,
               pClientSTA->wSTADesc.ucBcastSig));
    vos_pkt_return_packet(vosTempBuff); 
  }
  else
  {
    if ( NULL == pClientSTA->vosBegCachedFrame )
    {
      /*this is the first frame that we are caching */
      pClientSTA->vosBegCachedFrame = vosTempBuff;

      pClientSTA->tlCacheInfo.cacheInitTime = vos_timer_get_system_time();
      pClientSTA->tlCacheInfo.cacheDoneTime =
              pClientSTA->tlCacheInfo.cacheInitTime;
      pClientSTA->tlCacheInfo.cacheSize = 1;

      MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_CACHE_FRAME,
                       ucSTAId, pClientSTA->tlCacheInfo.cacheSize));

    }
    else
    {
      /*this is a subsequent frame that we are caching: chain to the end */
      vos_pkt_chain_packet(pClientSTA->vosEndCachedFrame,
                           vosTempBuff, VOS_TRUE);

      pClientSTA->tlCacheInfo.cacheDoneTime = vos_timer_get_system_time();
      pClientSTA->tlCacheInfo.cacheSize ++;

      if (pClientSTA->tlCacheInfo.cacheSize % WLANTL_CACHE_TRACE_WATERMARK == 0)
      {
        VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                  "%s: Cache High watermark for staid:%d (%d)",
                  __func__,ucSTAId, pClientSTA->tlCacheInfo.cacheSize);
        MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_CACHE_FRAME,
                         ucSTAId, pClientSTA->tlCacheInfo.cacheSize));
      }
    }
    pClientSTA->vosEndCachedFrame = vosTempBuff;
  }/*else new packet*/

  return VOS_STATUS_SUCCESS; 
}/*WLANTL_CacheSTAFrame*/

/*==========================================================================

  FUNCTION    WLANTL_FlushCachedFrames

  DESCRIPTION
    Internal utility function used by TL to flush the station cache

  DEPENDENCIES
    TL must be initiailized before this function gets called.
    
  PARAMETERS

    IN

    vosDataBuff:   it will contain a pointer to the first cached buffer
                   received,

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

  NOTE
    This function doesn't re-initialize vosDataBuff to NULL. It's caller's 
    responsibility to do so, if required, after this function call.
    Because of this restriction, we decide to make this function to static
    so that upper layer doesn't need to be aware of this restriction. 
    
============================================================================*/
static VOS_STATUS
WLANTL_FlushCachedFrames
(
  vos_pkt_t*      vosDataBuff
)
{
  /*----------------------------------------------------------------------
    Return the entire chain to vos if there are indeed cache frames 
  ----------------------------------------------------------------------*/
  if ( NULL != vosDataBuff )
  {
    vos_pkt_return_packet(vosDataBuff);
  }

  return VOS_STATUS_SUCCESS;  
}/*WLANTL_FlushCachedFrames*/

/*==========================================================================

  FUNCTION    WLANTL_ForwardSTAFrames

  DESCRIPTION
    Internal utility function for either forwarding cached data to the station after
    the station has been registered, or flushing cached data if the station has not 
    been registered. 
     

  DEPENDENCIES
    TL must be initiailized before this function gets called.
   
  PARAMETERS

    IN
    
    pTLCb:                  TL control block
    ucSTAId:                station id

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS
    This function doesn't re-initialize vosDataBuff to NULL. It's caller's 
    responsibility to do so, if required, after this function call.
    Because of this restriction, we decide to make this function to static
    so that upper layer doesn't need to be aware of this restriction. 

============================================================================*/
static VOS_STATUS
WLANTL_ForwardSTAFrames
(
  void*             pvosGCtx,
  v_U8_t            ucSTAId,
  v_U8_t            ucUcastSig,
  v_U8_t            ucBcastSig
)
{
  WLANTL_CbType*  pTLCb = NULL;
  WLANTL_STAClientType* pClientSTA = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------  
     Sanity check 
   -------------------------------------------------------------------------*/ 
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL: Invalid input pointer on WLANTL_ForwardSTAFrames TL %p",
         pTLCb ));
    return VOS_STATUS_E_FAULT;
  }

  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WLAN TL:Invalid station id requested on WLANTL_ForwardSTAFrames"));
    return VOS_STATUS_E_FAULT;
  }

  //WLAN_TL_LOCK_STA_CACHE(pTLCb->atlSTAClients[ucSTAId]); 

  /*------------------------------------------------------------------------
     Check if station has not been registered in the mean while
     if not registered, flush cached frames.
   ------------------------------------------------------------------------*/ 
  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pClientSTA->ucExists )
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
       "WLAN TL:Station has been deleted for STA %d - flushing cache", ucSTAId));
    MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_FLUSH_CACHED_FRAMES,
                     ucSTAId, pClientSTA->tlCacheInfo.cacheSize));
    WLANTL_FlushCachedFrames(pClientSTA->vosBegCachedFrame);
    goto done; 
  }

  /*------------------------------------------------------------------------
    Forwarding cache frames received while the station was in the process   
    of being registered with the rest of the SW components   

    Access to the cache must be locked; similarly updating the signature and   
    the existence flag must be synchronized because these values are checked   
    during cached  
  ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
       "WLAN TL:Preparing to fwd packets for STA %d", ucSTAId));

  /*-----------------------------------------------------------------------
    Save the new signature values
  ------------------------------------------------------------------------*/
  pClientSTA->wSTADesc.ucUcastSig  = ucUcastSig;
  pClientSTA->wSTADesc.ucBcastSig  = ucBcastSig;

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
       "WLAN TL:Fwd-ing packets for STA %d UC %d BC %d",
       ucSTAId, ucUcastSig, ucBcastSig));

  /*-------------------------------------------------------------------------  
     Check to see if we have any cached data to forward 
   -------------------------------------------------------------------------*/ 
  if ( NULL != pClientSTA->vosBegCachedFrame )
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL: Fwd-ing Cached packets for station %d", ucSTAId ));

    WLANTL_RxCachedFrames( pTLCb, 
                           ucSTAId, 
                           pClientSTA->vosBegCachedFrame);
  }
  else
  {
     TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL: NO cached packets for station %d", ucSTAId ));
  }

done:
  /*-------------------------------------------------------------------------  
   Clear the station cache 
   -------------------------------------------------------------------------*/
  pClientSTA->vosBegCachedFrame = NULL;
  pClientSTA->vosEndCachedFrame = NULL;
  pClientSTA->tlCacheInfo.cacheSize = 0;
  pClientSTA->tlCacheInfo.cacheClearTime = vos_timer_get_system_time();

    /*-----------------------------------------------------------------------
    After all the init is complete we can mark the existance flag 
    ----------------------------------------------------------------------*/
  pClientSTA->enableCaching = 0;

  //WLAN_TL_UNLOCK_STA_CACHE(pTLCb->atlSTAClients[ucSTAId]); 
  return VOS_STATUS_SUCCESS; 

}/*WLANTL_ForwardSTAFrames*/


#if defined(FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_ESE_UPLOAD)
/*==========================================================================

  FUNCTION    WLANTL_IsIAPPFrame

  DESCRIPTION
    Internal utility function for detecting incoming ESE IAPP frames

  DEPENDENCIES

  PARAMETERS

    IN
    
    pvBDHeader:             pointer to the BD header
    vosTempBuff:            the data packet

    IN/OUT
    pFirstDataPktArrived:   static from caller function; used for rssi 
                            computation
  RETURN VALUE
    The result code associated with performing the operation

    VOS_TRUE:   It is a IAPP frame
    VOS_FALSE:  It is NOT IAPP frame

  SIDE EFFECTS

============================================================================*/
v_BOOL_t
WLANTL_IsIAPPFrame
(
  v_PVOID_t         pvBDHeader,
  vos_pkt_t*        vosTempBuff
)
{
  v_U16_t             usMPDUDOffset;
  v_U8_t              ucOffset;
  v_U8_t              ucSnapHdr[WLANTL_LLC_SNAP_SIZE];
  v_SIZE_t            usSnapHdrSize = WLANTL_LLC_SNAP_SIZE;
  VOS_STATUS          vosStatus;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Check if OUI field is present.
  -------------------------------------------------------------------------*/
  if ( VOS_FALSE == WDA_IS_RX_LLC_PRESENT(pvBDHeader) )
  {
      TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                  "WLAN TL:LLC header removed, cannot determine BT-AMP type -"
                  "dropping pkt"));
      /* Drop packet */
      vos_pkt_return_packet(vosTempBuff);
      return VOS_TRUE;
  }
  usMPDUDOffset = (v_U8_t)WDA_GET_RX_MPDU_DATA_OFFSET(pvBDHeader);
  ucOffset      = (v_U8_t)usMPDUDOffset + WLANTL_LLC_SNAP_OFFSET;

  vosStatus = vos_pkt_extract_data( vosTempBuff, ucOffset,
                                (v_PVOID_t)ucSnapHdr, &usSnapHdrSize);

  if (( VOS_STATUS_SUCCESS != vosStatus)) 
  {
    TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                "Unable to extract Snap Hdr of data  packet -"
                "dropping pkt"));
    return VOS_FALSE;
  }

 /*------------------------------------------------------------------------
    Check if this is IAPP frame by matching Aironet Snap hdr.
  -------------------------------------------------------------------------*/
  // Compare returns 1 if values are same and 0
  // if not the same.
  if (( WLANTL_LLC_SNAP_SIZE != usSnapHdrSize ) ||
     ( 0 == vos_mem_compare(ucSnapHdr, (v_PVOID_t)WLANTL_AIRONET_SNAP_HEADER,
                            WLANTL_LLC_SNAP_SIZE ) ))
  {
    return VOS_FALSE;
  }

  return VOS_TRUE;

}
#endif //FEATURE_WLAN_ESE

/*==========================================================================

  FUNCTION    WLANTL_ProcessBAPFrame

  DESCRIPTION
    Internal utility function for processing incoming BT-AMP frames

  DEPENDENCIES
    TL must be initiailized before this function gets called.
    Bothe the BT-AMP station and the BAP Ctrl path must have been previously 
    registered with TL.

  PARAMETERS

    IN
    
    pvBDHeader:             pointer to the BD header
    vosTempBuff:            the data packet
    pTLCb:                  TL control block
    ucSTAId:                station id

    IN/OUT
    pFirstDataPktArrived:   static from caller function; used for rssi 
                            computation
  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input parameters are invalid
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
v_BOOL_t
WLANTL_ProcessBAPFrame
(
  v_PVOID_t         pvBDHeader,
  vos_pkt_t*        vosTempBuff,
  WLANTL_CbType*    pTLCb,
  v_U8_t*           pFirstDataPktArrived,
  v_U8_t            ucSTAId
)
{
  v_U16_t             usMPDUDOffset;
  v_U8_t              ucOffset;
  v_U8_t              ucOUI[WLANTL_LLC_OUI_SIZE];
  v_SIZE_t            usOUISize = WLANTL_LLC_OUI_SIZE;
  VOS_STATUS          vosStatus;
  v_U16_t             usType;
  v_SIZE_t            usTypeLen = sizeof(usType);
  v_U8_t              ucMPDUHOffset;
  v_U8_t              ucMPDUHLen = 0;
  v_U16_t             usActualHLen = 0;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Extract OUI and type from LLC and validate; if non-data send to BAP
  -------------------------------------------------------------------------*/
  if ( VOS_FALSE == WDA_IS_RX_LLC_PRESENT(pvBDHeader) )
  {
    TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
          "WLAN TL:LLC header removed, cannot determine BT-AMP type -"
              "dropping pkt"));
    /* Drop packet */
    vos_pkt_return_packet(vosTempBuff);
    return VOS_TRUE; 
  }

  usMPDUDOffset = (v_U8_t)WDA_GET_RX_MPDU_DATA_OFFSET(pvBDHeader);
  ucMPDUHOffset = (v_U8_t)WDA_GET_RX_MPDU_HEADER_OFFSET(pvBDHeader);
  ucMPDUHLen    = (v_U8_t)WDA_GET_RX_MPDU_HEADER_LEN(pvBDHeader);
  ucOffset      = (v_U8_t)usMPDUDOffset + WLANTL_LLC_OUI_OFFSET;

  vosStatus = vos_pkt_extract_data( vosTempBuff, ucOffset,
                                (v_PVOID_t)ucOUI, &usOUISize);

#if 0
  // Compare returns 1 if values are same and 0
  // if not the same.
  if (( WLANTL_LLC_OUI_SIZE != usOUISize ) ||
     ( 0 == vos_mem_compare(ucOUI, (v_PVOID_t)WLANTL_BT_AMP_OUI,
                            WLANTL_LLC_OUI_SIZE ) ))
  {
    TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                "LLC header points to diff OUI in BT-AMP station -"
                "dropping pkt"));
    /* Drop packet */
    vos_pkt_return_packet(vosTempBuff);
    return VOS_TRUE;
  }
#endif
  /*------------------------------------------------------------------------
    Extract LLC OUI and ensure that this is indeed a BT-AMP frame
   ------------------------------------------------------------------------*/
  vosStatus = vos_pkt_extract_data( vosTempBuff,
                                 ucOffset + WLANTL_LLC_OUI_SIZE,
                                (v_PVOID_t)&usType, &usTypeLen);

  if (( VOS_STATUS_SUCCESS != vosStatus) ||
      ( sizeof(usType) != usTypeLen ))
  {
    TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                "Unable to extract type on incoming BAP packet -"
                "dropping pkt"));
    /* Drop packet */
    vos_pkt_return_packet(vosTempBuff);
    return VOS_TRUE;
  }

  /*------------------------------------------------------------------------
    Check if this is BT-AMP data or ctrl packet(RSN, LinkSvision, ActivityR)
   ------------------------------------------------------------------------*/
  usType = vos_be16_to_cpu(usType);

  if (WLANTL_BAP_IS_NON_DATA_PKT_TYPE(usType))
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
      "WLAN TL:Non-data packet received over BT-AMP link: %d, => BAP",
               usType));

    /*Flatten packet as BAP expects to be able to peek*/
    if ( VOS_STATUS_SUCCESS != vos_pkt_flatten_rx_pkt(&vosTempBuff))
    {
      TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                 "WLAN TL:Cannot flatten BT-AMP packet - dropping"));
      /* Drop packet */
      vos_pkt_return_packet(vosTempBuff);
      return VOS_TRUE;
    }

    /* Send packet to BAP client*/
    if ( VOS_STATUS_SUCCESS != WDA_DS_TrimRxPacketInfo( vosTempBuff ) )
    {
      TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
        "WLAN TL:BD header corrupted - dropping packet"));
      /* Drop packet */
      vos_pkt_return_packet(vosTempBuff);
      return VOS_TRUE;
    }

    if ( 0 == WDA_GET_RX_FT_DONE(pvBDHeader) )
    {
      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
          "Non-data packet received over BT-AMP link: Sending it for "
          "frame Translation"));

      if (usMPDUDOffset > ucMPDUHOffset)
      {
        usActualHLen = usMPDUDOffset - ucMPDUHOffset;
      }

      /* software frame translation for BTAMP WDS.*/
      WLANTL_Translate80211To8023Header( vosTempBuff, &vosStatus, usActualHLen,
                                         ucMPDUHLen, pTLCb,ucSTAId, VOS_FALSE);
      
    }
    if (pTLCb->tlBAPClient.pfnTlBAPRx)
        pTLCb->tlBAPClient.pfnTlBAPRx( vos_get_global_context(VOS_MODULE_ID_TL,pTLCb),
                                       vosTempBuff,
                                       (WLANTL_BAPFrameEnumType)usType );
    else
    {
        VOS_ASSERT(0);
    }

    return VOS_TRUE;
  }
  else
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
      "WLAN TL: BAP DATA packet received over BT-AMP link: %d, => BAP",
               usType));
   /*!!!FIX ME!!*/
 #if 0
    /*--------------------------------------------------------------------
     For data packet collect phy stats RSSI and Link Quality
     Calculate the RSSI average and save it. Continuous average is done.
    --------------------------------------------------------------------*/
    if ( *pFirstDataPktArrived == 0)
    {
      pTLCb->atlSTAClients[ucSTAId].rssiAvg =
         WLANHAL_GET_RSSI_AVERAGE( pvBDHeader );
      pTLCb->atlSTAClients[ucSTAId].uLinkQualityAvg = 
        WLANHAL_RX_BD_GET_SNR( pvBDHeader );

      // Rcvd 1st pkt, start average from next time
      *pFirstDataPktArrived = 1;
    }
    else
    {
      pTLCb->atlSTAClients[ucSTAId].rssiAvg =
          (WLANHAL_GET_RSSI_AVERAGE( pvBDHeader ) + 
           pTLCb->atlSTAClients[ucSTAId].rssiAvg)/2;
      pTLCb->atlSTAClients[ucSTAId].uLinkQualityAvg =
          (WLANHAL_RX_BD_GET_SNR( pvBDHeader ) +  
           pTLCb->atlSTAClients[ucSTAId].uLinkQualityAvg)/2;
    }/*Else, first data packet*/
 #endif
  }/*BT-AMP data packet*/

  return VOS_FALSE; 
}/*WLANTL_ProcessBAPFrame*/


/*==========================================================================

  FUNCTION    WLANTL_ProcessFCFrame

  DESCRIPTION
    Internal utility function for processing incoming Flow Control frames. Enable
    or disable LWM mode based on the information.

  DEPENDENCIES
    TL must be initiailized before this function gets called.
    FW sends up special flow control frame.

  PARAMETERS

    IN
    pvosGCtx                pointer to vos global context
    pvBDHeader:             pointer to the BD header
    pTLCb:                  TL control block
    pvBDHeader              pointer to BD header.

    IN/OUT
    pFirstDataPktArrived:   static from caller function; used for rssi 
                            computation
  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input frame are invalid
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS
    The ingress and egress of each station will be updated. If needed, LWM mode will
    be enabled or disabled based on the flow control algorithm.

============================================================================*/
v_BOOL_t
WLANTL_ProcessFCFrame
(
  v_PVOID_t         pvosGCtx,
  vos_pkt_t*        pvosDataBuff,
  v_PVOID_t         pvBDHeader
)
{
#if 1 //enable processing of only fcStaTxDisabled bitmap for now. the else part is old better qos code.
      // need to revisit the old code for full implementation.
  v_U8_t            ucSTAId;
  v_U16_t           ucStaValidBitmap;
  v_U16_t           ucStaTxDisabledBitmap;
  WLANTL_CbType*    pTLCb = NULL;
  #ifdef TL_DEBUG_FC
  v_U32_t           rxTimeStamp;
  v_U32_t           curTick;
  #endif
  /*------------------------------------------------------------------------
   Extract TL control block
  ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_SuspendDataTx"));
    return VOS_STATUS_E_FAULT;
  }
  ucStaValidBitmap = WDA_GET_RX_FC_VALID_STA_MASK(pvBDHeader);
  ucStaTxDisabledBitmap = WDA_GET_RX_FC_STA_TX_DISABLED_BITMAP(pvBDHeader);
#ifdef TL_DEBUG_FC
  rxTimeStamp = WDA_GET_RX_TIMESTAMP(pvBDHeader);
  /* hard code of MTU_GLOBAL_TIMER_ADDR to calculate the time between generated and processed */
  wpalReadRegister(0x03081400+0x1D4, &curTick);

  TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "%ld (%ld-%ld): Disabled %x Valid %x", curTick > rxTimeStamp ? curTick - rxTimeStamp : rxTimeStamp - (0xFFFFFFFF - curTick),
    curTick, rxTimeStamp,  ucStaTxDisabledBitmap, ucStaValidBitmap));
#endif
  for(ucSTAId = 0; ucStaValidBitmap != 0; ucStaValidBitmap >>=1, ucStaTxDisabledBitmap >>= 1, ucSTAId ++)
  {
    if ( (0 == (ucStaValidBitmap & 0x1)) || (pTLCb->atlSTAClients[ucSTAId] && (0 == pTLCb->atlSTAClients[ucSTAId]->ucExists)) )
        continue;

    if (ucStaTxDisabledBitmap & 0x1)
    {
      WLANTL_SuspendDataTx(pvosGCtx, &ucSTAId, NULL);
    }
    else
    {
      WLANTL_ResumeDataTx(pvosGCtx, &ucSTAId);
    }
  }

#else
  VOS_STATUS          vosStatus;
  tpHalFcRxBd         pvFcRxBd = NULL;
  v_U8_t              ucBitCheck = 0x1;
  v_U8_t              ucStaValid = 0;
  v_U8_t              ucSTAId = 0;

      VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                 "Received FC Response");
  if ( (NULL == pTLCb) || (NULL == pvosDataBuff))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL:Invalid pointer in %s", __func__));
    return VOS_STATUS_E_FAULT;
  }
  vosStatus = vos_pkt_peek_data( pvosDataBuff, 0, (v_PVOID_t)&pvFcRxBd,
                                   sizeof(tHalFcRxBd));

  if ( (VOS_STATUS_SUCCESS != vosStatus) || (NULL == pvFcRxBd) )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "WLAN TL:wrong FC Rx packet"));
      return VOS_STATUS_E_INVAL;
  }
  
  // need to swap bytes in the FC contents.  
  WLANHAL_SwapFcRxBd(&pvFcRxBd->fcSTATxQLen[0]);

  //logic to enable/disable LWM mode for each station
  for( ucStaValid = (v_U8_t)pvFcRxBd->fcSTAValidMask; ucStaValid; ucStaValid >>= 1, ucBitCheck <<= 1, ucSTAId ++)
  {
    if ( (0 == (ucStaValid & 0x1)) || (0 == pTLCb->atlSTAClients[ucSTAId].ucExists) )
    {
      continue;
    }

    if ( pvFcRxBd->fcSTAThreshIndMask & ucBitCheck )
    {
      //LWM event is reported by FW. Able to fetch more packet
      if( pTLCb->atlSTAClients[ucSTAId].ucLwmModeEnabled )
      {
        //Now memory usage is below LWM. Station can send more packets.
        pTLCb->atlSTAClients[ucSTAId].ucLwmEventReported = TRUE;
      }
      else
      {
        TLLOG4(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
                 "WLAN TL: FW report LWM event but the station %d is not in LWM mode", ucSTAId));
      }
    }

    //calculate uEgress_length/uIngress_length only after receiving enough packets
    if (WLANTL_LWM_INGRESS_SAMPLE_THRESHOLD <= pTLCb->atlSTAClients[ucSTAId].uIngress_length)
    {
      //check memory usage info to see whether LWM mode should be enabled for the station
      v_U32_t uEgress_length = pTLCb->atlSTAClients[ucSTAId].uIngress_length + 
        pTLCb->atlSTAClients[ucSTAId].bmuMemConsumed - pvFcRxBd->fcSTATxQLen[ucSTAId];

      //if ((float)uEgress_length/(float)pTLCb->atlSTAClients[ucSTAId].uIngress_length 
      //      <= WLANTL_LWM_EGRESS_INGRESS_THRESHOLD)
      if ( (pTLCb->atlSTAClients[ucSTAId].uIngress_length > uEgress_length) &&
           ((pTLCb->atlSTAClients[ucSTAId].uIngress_length - uEgress_length ) >= 
            (pTLCb->atlSTAClients[ucSTAId].uIngress_length >> 2))
         )
      {   
         TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                    "WLAN TL:Enable LWM mode for station %d", ucSTAId));
         pTLCb->atlSTAClients[ucSTAId].ucLwmModeEnabled = TRUE;
      }
      else
      {
        if( pTLCb->atlSTAClients[ucSTAId].ucLwmModeEnabled )
        {
          TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                    "WLAN TL:Disable LWM mode for station %d", ucSTAId));
          pTLCb->atlSTAClients[ucSTAId].ucLwmModeEnabled = FALSE;
        }

      }

      //remember memory usage in FW starting from this round
      pTLCb->atlSTAClients[ucSTAId].bmuMemConsumed = pvFcRxBd->fcSTATxQLen[ucSTAId];
      pTLCb->atlSTAClients[ucSTAId].uIngress_length = 0;
    } //(WLANTL_LWM_INGRESS_SAMPLE_THRESHOLD <= pTLCb->atlSTAClients[ucSTAId].uIngress_length)

    if( pTLCb->atlSTAClients[ucSTAId].ucLwmModeEnabled )
    {
      //always update current maximum allowed memeory usage
      pTLCb->atlSTAClients[ucSTAId].uBuffThresholdMax =  WLANTL_STA_BMU_THRESHOLD_MAX -
        pvFcRxBd->fcSTATxQLen[ucSTAId];
    }

  }
#endif

  return VOS_STATUS_SUCCESS;
}


/*==========================================================================

  FUNCTION    WLANTL_RxFrames

  DESCRIPTION
    Callback registered by TL and called by BAL when a packet is received
    over the bus. Upon the call of this function TL will make the necessary
    decision with regards to the forwarding or queuing of this packet and
    the layer it needs to be delivered to.

  DEPENDENCIES
    TL must be initiailized before this function gets called.
    If the frame carried is a data frame then the station for which it is
    destined to must have been previously registered with TL.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or BAL's control block can be extracted from its context

    vosDataBuff:   it will contain a pointer to the first buffer received,
                    if there is more then one packet they will be chained
                    using vOSS buffers.

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input parameters are invalid
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_RxFrames
(
  v_PVOID_t      pvosGCtx,
  vos_pkt_t     *pFrameDataBuff
)
{
  vos_pkt_t*          vosDataBuff = (vos_pkt_t*)pFrameDataBuff;
  WLANTL_CbType*      pTLCb = NULL;
  WLANTL_STAClientType* pClientSTA = NULL;
  WLANTL_STAFuncType  pfnSTAFsm;
  vos_pkt_t*          vosTempBuff;
  v_U8_t              ucSTAId;
  VOS_STATUS          vosStatus;
  v_U8_t              ucFrmType;
  v_PVOID_t           pvBDHeader = NULL;
  WLANTL_STAEventType wSTAEvent  = WLANTL_RX_EVENT;
  v_U8_t              ucTid      = 0;
  v_BOOL_t            broadcast  = VOS_FALSE;
  v_BOOL_t            selfBcastLoopback = VOS_FALSE;
  static v_U8_t       first_data_pkt_arrived;
  v_U32_t             uDPUSig; 
  v_U16_t             usPktLen;
  v_BOOL_t            bForwardIAPPwithLLC = VOS_FALSE;
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
  v_S7_t              currentAvgRSSI = 0;
  v_U8_t              ac;

#endif

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:TL Receive Frames called"));

  /*------------------------------------------------------------------------
    Sanity check
    ------------------------------------------------------------------------*/
  if ( NULL == vosDataBuff )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_RxFrames"));
    return VOS_STATUS_E_INVAL;
  }

 /*------------------------------------------------------------------------
   Popolaute timestamp as the time when packet arrives
   ---------------------------------------------------------------------- */
   vosDataBuff->timestamp = vos_timer_get_system_ticks();

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_FAULT;
  }

  /*---------------------------------------------------------------------
    Save the initial buffer - this is the first received buffer
   ---------------------------------------------------------------------*/
  vosTempBuff = vosDataBuff;

  while ( NULL != vosTempBuff )
  {
    broadcast = VOS_FALSE;
    selfBcastLoopback = VOS_FALSE; 

    vos_pkt_walk_packet_chain( vosDataBuff, &vosDataBuff, 1/*true*/ );

    if( vos_get_conparam() == VOS_MONITOR_MODE )
      {
         if( pTLCb->isConversionReq )
            WLANTL_MonTranslate80211To8023Header(vosTempBuff, pTLCb);

         pTLCb->pfnMonRx(pvosGCtx, vosTempBuff, pTLCb->isConversionReq);
         vosTempBuff = vosDataBuff;
         continue;
      }

    /*---------------------------------------------------------------------
      Peek at BD header - do not remove
      !!! Optimize me: only part of header is needed; not entire one
     ---------------------------------------------------------------------*/
    vosStatus = WDA_DS_PeekRxPacketInfo( vosTempBuff, (v_PVOID_t)&pvBDHeader, 1/*Swap BD*/ );

    if ( NULL == pvBDHeader )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "WLAN TL:Cannot extract BD header"));
      /* Drop packet */
      vos_pkt_return_packet(vosTempBuff);
      vosTempBuff = vosDataBuff;
      continue;
    }

    /*---------------------------------------------------------------------
      Check if FC frame reported from FW
    ---------------------------------------------------------------------*/
    if(WDA_IS_RX_FC(pvBDHeader))
    {
      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLAN TL:receive one FC frame"));

      WLANTL_ProcessFCFrame(pvosGCtx, vosTempBuff, pvBDHeader);
      /* Drop packet */
      vos_pkt_return_packet(vosTempBuff);
      vosTempBuff = vosDataBuff;
      continue;
    }

    /* AMSDU HW bug fix
     * After 2nd AMSDU subframe HW could not handle BD correctly
     * HAL workaround is needed */
    if(WDA_GET_RX_ASF(pvBDHeader))
    {
      WDA_DS_RxAmsduBdFix(pvosGCtx, pvBDHeader);
    }

    /*---------------------------------------------------------------------
      Extract frame control field from 802.11 header if present 
      (frame translation not done) 
    ---------------------------------------------------------------------*/

    vosStatus = WDA_DS_GetFrameTypeSubType( pvosGCtx, vosTempBuff,
                         pvBDHeader, &ucFrmType );
    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                   "WLAN TL:Cannot extract Frame Control Field"));
      /* Drop packet */
      vos_pkt_return_packet(vosTempBuff);
      vosTempBuff = vosDataBuff;
      continue;
    }

    vos_pkt_get_packet_length(vosTempBuff, &usPktLen);

    /*---------------------------------------------------------------------
      Check if management and send to PE
    ---------------------------------------------------------------------*/

    if ( WLANTL_IS_MGMT_FRAME(ucFrmType))
    {
      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLAN TL:Sending packet to management client"));
      if ( VOS_STATUS_SUCCESS != vos_pkt_flatten_rx_pkt(&vosTempBuff))
      {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                   "WLAN TL:Cannot flatten packet - dropping"));
        /* Drop packet */
        vos_pkt_return_packet(vosTempBuff);
        vosTempBuff = vosDataBuff;
        continue;
      }
      ucSTAId = (v_U8_t)WDA_GET_RX_STAID( pvBDHeader );
      /* Read RSSI and update */
      if(!WLANTL_STA_ID_INVALID(ucSTAId))
      {
#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
        /* Read RSSI and update */
        vosStatus = WLANTL_HSHandleRXFrame(pvosGCtx,
                                           WLANTL_MGMT_FRAME_TYPE,
                                           pvBDHeader,
                                           ucSTAId,
                                           VOS_FALSE,
                                           NULL);
#else
        vosStatus = WLANTL_ReadRSSI(pvosGCtx, pvBDHeader, ucSTAId);
#endif
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
        {
          TLLOGW(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                           "Handle RX Management Frame fail within Handoff "
                           "support module"));
          /* Do Not Drop packet at here
           * Revisit why HO module return fail
           *   vos_pkt_return_packet(vosTempBuff);
           *   vosTempBuff = vosDataBuff;
           *   continue;
           */
        }
        vosStatus = WLANTL_ReadSNR(pvosGCtx, pvBDHeader, ucSTAId);

        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
        {
          TLLOGW(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                            FL("Failed to Read SNR")));
        }
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
        pClientSTA = pTLCb->atlSTAClients[ucSTAId];
        if ( NULL != pClientSTA)
        {
            pClientSTA->interfaceStats.mgmtRx++;
        }
#endif
      }

      pTLCb->tlMgmtFrmClient.pfnTlMgmtFrmRx( pvosGCtx, vosTempBuff); 
    }
    else /* Data Frame */
    {
      ucSTAId = (v_U8_t)WDA_GET_RX_STAID( pvBDHeader );
      ucTid   = (v_U8_t)WDA_GET_RX_TID( pvBDHeader );

      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLAN TL:Data packet received for STA %d", ucSTAId));

      /*------------------------------------------------------------------
        This should be corrected when multipe sta support is added !!!
        for now bcast frames will be sent to the last registered STA
       ------------------------------------------------------------------*/
      if ( WDA_IS_RX_BCAST(pvBDHeader))
      {
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
        "WLAN TL:TL rx Bcast frame - sending to last registered station"));
        broadcast = VOS_TRUE;
        /*-------------------------------------------------------------------
          If Addr1 is b/mcast, but Addr3 is our own self MAC, it is a b/mcast
          pkt we sent  looping back to us. To be dropped if we are non BTAMP  
         -------------------------------------------------------------------*/ 
        if( WLANHAL_RX_BD_ADDR3_SELF_IDX == 
            (v_U8_t)WDA_GET_RX_ADDR3_IDX( pvBDHeader )) 
        {
          selfBcastLoopback = VOS_TRUE; 
        }
      }/*if bcast*/

      if ((WLANTL_STA_ID_INVALID(ucSTAId)) || (WLANTL_TID_INVALID(ucTid)))
      {
        TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                   "WLAN TL:STAId %d, Tid %d. Invalid STA ID/TID- dropping pkt",
                   ucSTAId, ucTid));
        /* Drop packet */
        vos_pkt_return_packet(vosTempBuff);
        vosTempBuff = vosDataBuff;
        continue;
      }

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
      ac = WLANTL_TID_2_AC[ucTid];
#endif

      /*----------------------------------------------------------------------
        No need to lock cache access because cache manipulation only happens
        in the transport thread/task context
        - These frames are to be forwarded to the station upon registration
          which happens in the main thread context
          The caching here can happen in either Tx or Rx thread depending
          on the current SSC scheduling
        - also we need to make sure that the frames in the cache are fwd-ed to
          the station before the new incoming ones 
      -----------------------------------------------------------------------*/
      pClientSTA = pTLCb->atlSTAClients[ucSTAId];
      if (NULL == pClientSTA)
      {
          TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                   "WLAN TL:STA not allocated memory. Dropping packet"));
          vos_pkt_return_packet(vosTempBuff);
          vosTempBuff = vosDataBuff;
          continue;
      }

#ifdef FEATURE_WLAN_TDLS
      if (( pClientSTA->ucExists ) &&
           (WLAN_STA_TDLS == pClientSTA->wSTADesc.wSTAType) &&
           (pClientSTA->ucTxSuspended))
          vos_atomic_set_U8( &pClientSTA->ucTxSuspended, 0 );
      else if ( !broadcast && (pClientSTA->ucExists == 0 ) )
      {
          tpSirMacMgmtHdr pMacHeader = WDA_GET_RX_MAC_HEADER( pvBDHeader );

          /* from the direct peer while it is not registered to TL yet */
          if ( (pMacHeader->fc.fromDS == 0) &&
               (pMacHeader->fc.toDS == 0) )
          {
              v_U8_t ucAddr3STAId;

              ucAddr3STAId = WDA_GET_RX_ADDR3_IDX(pvBDHeader);

              if ( WLANTL_STA_ID_INVALID(ucAddr3STAId) )
              {
                TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                           "WLAN TL:STA ID %d invalid - dropping pkt", ucAddr3STAId));
                /* Drop packet */
                vos_pkt_return_packet(vosTempBuff);
                vosTempBuff = vosDataBuff;
                continue;
              }

              if (!(pTLCb->atlSTAClients[ucAddr3STAId] && pTLCb->atlSTAClients[ucAddr3STAId]->ucExists &&
                  (WLAN_STA_INFRA == pTLCb->atlSTAClients[ucAddr3STAId]->wSTADesc.wSTAType) &&
                  (WLANTL_STA_AUTHENTICATED == pTLCb->atlSTAClients[ucAddr3STAId]->tlState)))
              {
                  TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                           "%s: staId %d addr3Id %d tlState %d. Unkown Receiver/Transmitter Dropping packet", __func__,
                           ucSTAId, ucAddr3STAId, pTLCb->atlSTAClients[ucAddr3STAId]->tlState));
                  vos_pkt_return_packet(vosTempBuff);
                  vosTempBuff = vosDataBuff;
                  continue;
              }
              else
              {
                  TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                           "%s: staId %d doesn't exist, but mapped to AP staId %d", __func__,
                           ucSTAId, ucAddr3STAId));
                  ucSTAId = ucAddr3STAId;
                  pClientSTA = pTLCb->atlSTAClients[ucAddr3STAId];
              }
          }
      }
#endif

      if (( pClientSTA->enableCaching == 1 ) &&
            /*Dont buffer Broadcast/Multicast frames. If AP transmits bursts of Broadcast/Multicast data frames, 
             * libra buffers all Broadcast/Multicast packets after authentication with AP, 
             * So it will lead to low resource condition in Rx Data Path.*/
          ( WDA_IS_RX_BCAST(pvBDHeader) == 0 ))
      {
        if( WDA_IsSelfSTA(pvosGCtx,ucSTAId))
        {
           //drop packet for Self STA index
           TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                  "%s: Packet dropped for Self STA with staId %d ", __func__, ucSTAId ));

           vos_pkt_return_packet(vosTempBuff);
           vosTempBuff = vosDataBuff;
           continue;
        }
        uDPUSig = WDA_GET_RX_DPUSIG( pvBDHeader );
          //Station has not yet been registered with TL - cache the frame
        TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                 "%s: staId %d exist %d tlState %d cache rx frame", __func__, ucSTAId,
                 pClientSTA->ucExists, pClientSTA->tlState));
        WLANTL_CacheSTAFrame( pTLCb, ucSTAId, vosTempBuff, uDPUSig, broadcast, ucFrmType);
        vosTempBuff = vosDataBuff;
        continue;
      }

#ifdef FEATURE_WLAN_ESE_UPLOAD
      if ((pClientSTA->wSTADesc.ucIsEseSta)|| broadcast)
      {
        /*--------------------------------------------------------------------
          Filter the IAPP frames for ESE connection;
          if data it will return false and it
          will be routed through the regular data path
        --------------------------------------------------------------------*/
        if ( WLANTL_IsIAPPFrame(pvBDHeader,
                                vosTempBuff))
        {
            bForwardIAPPwithLLC = VOS_TRUE;
        }
      }
#endif

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
      if ((pClientSTA->wSTADesc.ucIsEseSta)|| broadcast)
      {
        /*--------------------------------------------------------------------
          Filter the IAPP frames for ESE connection;
          if data it will return false and it 
          will be routed through the regular data path
        --------------------------------------------------------------------*/
        if ( WLANTL_IsIAPPFrame(pvBDHeader,
                                vosTempBuff))
        {
            if ( VOS_STATUS_SUCCESS != vos_pkt_flatten_rx_pkt(&vosTempBuff))
            {
               TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                        "WLAN TL:Cannot flatten packet - dropping"));
               /* Drop packet */
               vos_pkt_return_packet(vosTempBuff);
            } else {

               TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                        "WLAN TL: Received ESE IAPP Frame"));

               pTLCb->tlMgmtFrmClient.pfnTlMgmtFrmRx( pvosGCtx, vosTempBuff); 
            }
            vosTempBuff = vosDataBuff;
            continue;
        }
      }
#endif  /* defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD) */

      if ( WLAN_STA_BT_AMP == pClientSTA->wSTADesc.wSTAType )
      {
        /*--------------------------------------------------------------------
          Process the ctrl BAP frame; if data it will return false and it 
          will be routed through the regular data path
        --------------------------------------------------------------------*/
        if ( WLANTL_ProcessBAPFrame( pvBDHeader,
                                     vosTempBuff,
                                     pTLCb,
                                    &first_data_pkt_arrived,
                                     ucSTAId))
        {
          vosTempBuff = vosDataBuff;
          continue;
        }
      }/*if BT-AMP station*/
      else if(selfBcastLoopback == VOS_TRUE)
      { 
        /* Drop packet */ 
        vos_pkt_return_packet(vosTempBuff); 
        vosTempBuff = vosDataBuff; 
        continue; 
      } 
      
      /*---------------------------------------------------------------------
        Data packet received, send to state machine
      ---------------------------------------------------------------------*/
      wSTAEvent = WLANTL_RX_EVENT;

      pfnSTAFsm = tlSTAFsm[pClientSTA->tlState].
                      pfnSTATbl[wSTAEvent];

      if ( NULL != pfnSTAFsm )
      {
#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
        /* Read RSSI and update */
        vosStatus = WLANTL_HSHandleRXFrame(pvosGCtx,
                                           WLANTL_DATA_FRAME_TYPE,
                                           pvBDHeader,
                                           ucSTAId,
                                           broadcast,
                                           vosTempBuff);
        broadcast = VOS_FALSE;
#else
        vosStatus = WLANTL_ReadRSSI(pvosGCtx, pvBDHeader, ucSTAId);
#endif
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
        {
          TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
            "Handle RX Data Frame fail within Handoff support module"));
          /* Do Not Drop packet at here 
           * Revisit why HO module return fail
           * vos_pkt_return_packet(vosTempBuff);
           * vosTempBuff = vosDataBuff;
           * continue;
           */
        }
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
        pClientSTA = pTLCb->atlSTAClients[ucSTAId];
        if ( NULL != pClientSTA)
        {
            tpSirMacMgmtHdr pMacHeader = WDA_GET_RX_MAC_HEADER( pvBDHeader );
            if (!IS_BROADCAST_ADD(pMacHeader->da) && IS_MULTICAST_ADD(pMacHeader->da))
            {
                pClientSTA->interfaceStats.accessCategoryStats[ac].rxMcast++;
            }

            WLANTL_HSGetDataRSSI(pvosGCtx, pvBDHeader, ucSTAId,
                    &currentAvgRSSI);
            pClientSTA->interfaceStats.rssiData = currentAvgRSSI;

            pClientSTA->interfaceStats.accessCategoryStats[ac].rxMpdu++;
            if (WDA_IS_RX_AN_AMPDU (pvBDHeader))
            {
                pClientSTA->interfaceStats.accessCategoryStats[ac].rxAmpdu++;
            }
        }


#endif
        vosStatus = WLANTL_ReadSNR(pvosGCtx, pvBDHeader, ucSTAId);

        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
        {
          TLLOGW(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                           FL("Failed to Read SNR")));
        }

        pfnSTAFsm( pvosGCtx, ucSTAId, &vosTempBuff, bForwardIAPPwithLLC);
      }
      else
        {
          TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
            "WLAN TL:NULL state function, STA:%d, State: %d- dropping packet",
                   ucSTAId, pClientSTA->tlState));
          /* Drop packet */
          vos_pkt_return_packet(vosTempBuff);
          vosTempBuff = vosDataBuff;
          continue;
        }
    }/* else data frame*/

    vosTempBuff = vosDataBuff;
  }/*while chain*/

  return VOS_STATUS_SUCCESS;
}/* WLANTL_RxFrames */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
/*==========================================================================

  FUNCTION    WLANTL_CollectInterfaceStats

  DESCRIPTION
    Utility function used by TL to send the statitics

  DEPENDENCIES


  PARAMETERS

    IN

    ucSTAId:    station for which the statistics need to collected

    vosDataBuff: it will contain the pointer to the corresponding
                structure

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input parameters are invalid
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_CollectInterfaceStats
(
  v_PVOID_t       pvosGCtx,
  v_U8_t          ucSTAId,
  WLANTL_InterfaceStatsType    *vosDataBuff
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Invalid station id requested on WLANTL_CollectStats"));
    return VOS_STATUS_E_FAULT;
  }
  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
       "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_CollectStats"));
    return VOS_STATUS_E_FAULT;
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
              "WLAN TL: collect WIFI_STATS_IFACE results"));

  vos_mem_copy(vosDataBuff, &pTLCb->atlSTAClients[ucSTAId]->interfaceStats,
          sizeof(WLANTL_InterfaceStatsType));
  return VOS_STATUS_SUCCESS;
}

/*==========================================================================

  FUNCTION    WLANTL_ClearInterfaceStats

  DESCRIPTION
    Utility function used by TL to clear the statitics

  DEPENDENCIES


  PARAMETERS

    IN

    ucSTAId:    station for which the statistics need to collected

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input parameters are invalid
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_ClearInterfaceStats
(
  v_PVOID_t       pvosGCtx,
  v_U8_t          ucSTAId,
  v_U8_t          statsClearReqMask
)
{
    WLANTL_CbType*  pTLCb = NULL;
    WLANTL_STAClientType* pClientSTA = NULL;
    /*------------------------------------------------------------------------
      Sanity check
      ------------------------------------------------------------------------*/
    if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
    {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid station id requested on WLANTL_CollectStats"));
        return VOS_STATUS_E_FAULT;
    }
    /*------------------------------------------------------------------------
      Extract TL control block
      ------------------------------------------------------------------------*/
    pTLCb = VOS_GET_TL_CB(pvosGCtx);
    if ( NULL == pTLCb )
    {
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_CollectStats"));
        return VOS_STATUS_E_FAULT;
    }

    pClientSTA = pTLCb->atlSTAClients[ucSTAId];
    if ( NULL == pClientSTA )
    {
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                    "WLAN TL:Client Memory was not allocated on %s", __func__));
        return VOS_STATUS_E_FAILURE;
    }

    if ((statsClearReqMask & WIFI_STATS_IFACE_AC) ||
            (statsClearReqMask & WIFI_STATS_IFACE)) {
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                    "WLAN TL:cleared WIFI_STATS_IFACE_AC results"));
        pClientSTA->interfaceStats.accessCategoryStats[0].rxMcast = 0;
        pClientSTA->interfaceStats.accessCategoryStats[1].rxMcast = 0;
        pClientSTA->interfaceStats.accessCategoryStats[2].rxMcast = 0;
        pClientSTA->interfaceStats.accessCategoryStats[3].rxMcast = 0;

        pClientSTA->interfaceStats.accessCategoryStats[0].rxMpdu = 0;
        pClientSTA->interfaceStats.accessCategoryStats[1].rxMpdu = 0;
        pClientSTA->interfaceStats.accessCategoryStats[2].rxMpdu = 0;
        pClientSTA->interfaceStats.accessCategoryStats[3].rxMpdu = 0;

        pClientSTA->interfaceStats.accessCategoryStats[0].rxAmpdu = 0;
        pClientSTA->interfaceStats.accessCategoryStats[1].rxAmpdu = 0;
        pClientSTA->interfaceStats.accessCategoryStats[2].rxAmpdu = 0;
        pClientSTA->interfaceStats.accessCategoryStats[3].rxAmpdu = 0;
    }

    if (statsClearReqMask & WIFI_STATS_IFACE) {
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                    "WLAN TL:cleared WIFI_STATS_IFACE results"));
        pClientSTA->interfaceStats.mgmtRx = 0;
        pClientSTA->interfaceStats.rssiData = 0;
        return VOS_STATUS_SUCCESS;
    }

    return VOS_STATUS_SUCCESS;
}

#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

/*==========================================================================

  FUNCTION    WLANTL_RxCachedFrames

  DESCRIPTION
    Utility function used by TL to forward the cached frames to a particular
    station; 

  DEPENDENCIES
    TL must be initiailized before this function gets called.
    If the frame carried is a data frame then the station for which it is
    destined to must have been previously registered with TL.

  PARAMETERS

    IN
    pTLCb:   pointer to TL handle 
   
    ucSTAId:    station for which we need to forward the packets

    vosDataBuff:   it will contain a pointer to the first cached buffer
                   received, if there is more then one packet they will be
                   chained using vOSS buffers.

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input parameters are invalid
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_RxCachedFrames
(
  WLANTL_CbType*  pTLCb,
  v_U8_t          ucSTAId,
  vos_pkt_t*      vosDataBuff
)
{
  WLANTL_STAClientType* pClientSTA = NULL;
  WLANTL_STAFuncType  pfnSTAFsm;
  vos_pkt_t*          vosTempBuff;
  VOS_STATUS          vosStatus;
  v_PVOID_t           pvBDHeader = NULL;
  WLANTL_STAEventType wSTAEvent  = WLANTL_RX_EVENT;
  v_U8_t              ucTid      = 0;
  v_BOOL_t            broadcast  = VOS_FALSE;
  v_BOOL_t            bSigMatch  = VOS_FALSE; 
  v_BOOL_t            selfBcastLoopback = VOS_FALSE;
  static v_U8_t       first_data_pkt_arrived;
  v_U32_t             uDPUSig; 
  v_U8_t              ucUcastSig; 
  v_U8_t              ucBcastSig; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:TL Receive Cached Frames called"));

  /*------------------------------------------------------------------------
    Sanity check
    ------------------------------------------------------------------------*/
  if ( NULL == vosDataBuff )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_RxFrames"));
    return VOS_STATUS_E_INVAL;
  }

  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL:Client Memory was not allocated on %s", __func__));
    return VOS_STATUS_E_FAILURE;
  }

  MTRACE(vos_trace(VOS_MODULE_ID_TL, TRACE_CODE_TL_FORWARD_CACHED_FRAMES,
                   ucSTAId, 1<<16 | pClientSTA->tlCacheInfo.cacheSize));

  /*---------------------------------------------------------------------
    Save the initial buffer - this is the first received buffer
   ---------------------------------------------------------------------*/
  vosTempBuff = vosDataBuff;

  while ( NULL != vosTempBuff )
  {
    broadcast = VOS_FALSE;
    selfBcastLoopback = VOS_FALSE; 

    vos_pkt_walk_packet_chain( vosDataBuff, &vosDataBuff, 1/*true*/ );

          TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:Sending new cached packet to station %d", ucSTAId));
    /*---------------------------------------------------------------------
      Peek at BD header - do not remove
      !!! Optimize me: only part of header is needed; not entire one
     ---------------------------------------------------------------------*/
    vosStatus = WDA_DS_PeekRxPacketInfo( vosTempBuff, (v_PVOID_t)&pvBDHeader, 0 );

    if ( NULL == pvBDHeader )
          {
            TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "WLAN TL:Cannot extract BD header"));
          /* Drop packet */
          vos_pkt_return_packet(vosTempBuff);
          vosTempBuff = vosDataBuff;
          continue;
        }

    uDPUSig = WDA_GET_RX_DPUSIG( pvBDHeader );

    /* AMSDU HW bug fix
     * After 2nd AMSDU subframe HW could not handle BD correctly
     * HAL workaround is needed */
    if(WDA_GET_RX_ASF(pvBDHeader))
    {
      WDA_DS_RxAmsduBdFix(vos_get_global_context(VOS_MODULE_ID_TL,pTLCb), 
                           pvBDHeader);
    }

    ucTid   = (v_U8_t)WDA_GET_RX_TID( pvBDHeader );

    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:Data packet cached for STA %d", ucSTAId);

    /*------------------------------------------------------------------
      This should be corrected when multipe sta support is added !!!
      for now bcast frames will be sent to the last registered STA
     ------------------------------------------------------------------*/
    if ( WDA_IS_RX_BCAST(pvBDHeader))
    {
          TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLAN TL:TL rx Bcast frame "));
      broadcast = VOS_TRUE;

      /* If Addr1 is b/mcast, but Addr3 is our own self MAC, it is a b/mcast 
       * pkt we sent looping back to us. To be dropped if we are non BTAMP  
       */ 
      if( WLANHAL_RX_BD_ADDR3_SELF_IDX == 
          (v_U8_t)WDA_GET_RX_ADDR3_IDX( pvBDHeader )) 
      {
        selfBcastLoopback = VOS_TRUE; 
      }
    }/*if bcast*/

     /*-------------------------------------------------------------------------
      Check if the packet that we cached matches the DPU signature of the
      newly added station 
    -------------------------------------------------------------------------*/
    pClientSTA = pTLCb->atlSTAClients[ucSTAId];

    if ( NULL == pClientSTA )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
    }

    if ( broadcast )
    {
      ucBcastSig = (v_U8_t)uDPUSig;
      bSigMatch = (( WLAN_TL_INVALID_B_SIG != 
                    pClientSTA->wSTADesc.ucBcastSig ) &&
        ( ucBcastSig == pClientSTA->wSTADesc.ucBcastSig ));
    }
    else
    {
      ucUcastSig = (v_U8_t)uDPUSig;
      bSigMatch = (( WLAN_TL_INVALID_U_SIG != 
                      pClientSTA->wSTADesc.ucUcastSig ) &&
          ( ucUcastSig == pClientSTA->wSTADesc.ucUcastSig ));
    }

    /*-------------------------------------------------------------------------
      If the packet doesn't match - drop it 
    -------------------------------------------------------------------------*/
    if ( !bSigMatch )
    {
            TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_MED,
        "WLAN TL: Cached packet does not match DPU Sig of the new STA - drop "
        " DPU Sig %d  UC %d BC %d B %d",
        uDPUSig,
        pClientSTA->wSTADesc.ucUcastSig,
        pClientSTA->wSTADesc.ucBcastSig,
        broadcast));

      /* Drop packet */ 
      vos_pkt_return_packet(vosTempBuff); 
      vosTempBuff = vosDataBuff; 
      continue; 

    }/*if signature mismatch*/

    /*------------------------------------------------------------------------
      Check if BT-AMP frame:
      - additional processing needed in this case to separate BT-AMP date
        from BT-AMP Ctrl path 
    ------------------------------------------------------------------------*/
    if ( WLAN_STA_BT_AMP == pClientSTA->wSTADesc.wSTAType )
    {
      /*--------------------------------------------------------------------
        Process the ctrl BAP frame; if data it will return false and it 
        will be routed through the regular data path
      --------------------------------------------------------------------*/
      if ( WLANTL_ProcessBAPFrame( pvBDHeader,
                                   vosTempBuff,
                                   pTLCb,
                                  &first_data_pkt_arrived,
                                   ucSTAId))
      {
          vosTempBuff = vosDataBuff;
          continue;
        }
      }/*if BT-AMP station*/
      else if(selfBcastLoopback == VOS_TRUE)
      { 
        /* Drop packet */ 
        vos_pkt_return_packet(vosTempBuff); 
        vosTempBuff = vosDataBuff; 
        continue; 
      } 
      
      /*---------------------------------------------------------------------
        Data packet received, send to state machine
      ---------------------------------------------------------------------*/
      wSTAEvent = WLANTL_RX_EVENT;

      pfnSTAFsm = tlSTAFsm[pClientSTA->tlState].
                      pfnSTATbl[wSTAEvent];

      if ( NULL != pfnSTAFsm )
      {
#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
        /* Read RSSI and update */
        vosStatus = WLANTL_HSHandleRXFrame(vos_get_global_context(
                                         VOS_MODULE_ID_TL,pTLCb),
                                           WLANTL_DATA_FRAME_TYPE,
                                           pvBDHeader,
                                           ucSTAId,
                                           broadcast,
                                           vosTempBuff);
        broadcast = VOS_FALSE;
#else
        vosStatus = WLANTL_ReadRSSI(vos_get_global_context(VOS_MODULE_ID_TL,pTLCb), pvBDHeader, ucSTAId);
#endif
        if(!VOS_IS_STATUS_SUCCESS(vosStatus))
        {
          TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "Handle RX Data Frame fail within Handoff support module"));
          /* Do Not Drop packet at here 
           * Revisit why HO module return fail
          vos_pkt_return_packet(vosTempBuff);
          vosTempBuff = vosDataBuff;
          continue;
           */
        }
        pfnSTAFsm( vos_get_global_context(VOS_MODULE_ID_TL,pTLCb), ucSTAId, 
                 &vosTempBuff, VOS_FALSE);
      }
      else
      {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:NULL state function, STA:%d, State: %d- dropping packet",
                   ucSTAId, pClientSTA->tlState));
        /* Drop packet */
        vos_pkt_return_packet(vosTempBuff);
        vosTempBuff = vosDataBuff;
        continue;
      }

    vosTempBuff = vosDataBuff;
  }/*while chain*/

  return VOS_STATUS_SUCCESS;
}/* WLANTL_RxCachedFrames */

/*==========================================================================
  FUNCTION    WLANTL_RxProcessMsg

  DESCRIPTION
    Called by VOSS when a message was serialized for TL through the
    rx thread/task.

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    message:        type and content of the message


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  Other values can be returned as a result of a function call, please check
  corresponding API for more info.
  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_RxProcessMsg
(
  v_PVOID_t        pvosGCtx,
  vos_msg_t*       message
)
{
   VOS_STATUS      vosStatus = VOS_STATUS_SUCCESS;
   v_U32_t         uData;
   v_U8_t          ucSTAId;
   v_U8_t          ucUcastSig;
   v_U8_t          ucBcastSig;

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == message )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_RxProcessMessage"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Process message
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Received message: %d through rx flow", message->type));

  switch( message->type )
  {

  case WLANTL_RX_FWD_CACHED:
    /*---------------------------------------------------------------------
     The data sent with the message has the following structure:
       | 00 | ucBcastSignature | ucUcastSignature | ucSTAID |
       each field above is one byte
    ---------------------------------------------------------------------*/
    uData       = message->bodyval;
    ucSTAId     = ( uData & 0x000000FF);
    ucUcastSig  = ( uData & 0x0000FF00)>>8;
    ucBcastSig  = (v_U8_t)(( uData & 0x00FF0000)>>16);
    vosStatus   = WLANTL_ForwardSTAFrames( pvosGCtx, ucSTAId,
                                           ucUcastSig, ucBcastSig);
    break;

  default:
    /*no processing for now*/
    break;
  }

  return VOS_STATUS_SUCCESS;
}


/*==========================================================================
  FUNCTION    WLANTL_ResourceCB

  DESCRIPTION
    Called by the TL when it has packets available for transmission.

  DEPENDENCIES
    The TL must be registered with BAL before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or BAL's control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_ResourceCB
(
  v_PVOID_t       pvosGCtx,
  v_U32_t         uCount
)
{
   WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_FAULT;
  }

  pTLCb->uResCount = uCount;


  /*-----------------------------------------------------------------------
    Resume Tx if enough res and not suspended
   -----------------------------------------------------------------------*/
  if (( pTLCb->uResCount >=  WDA_TLI_MIN_RES_MF ) &&
      ( 0 == pTLCb->ucTxSuspended ))
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
         "WLAN TL:Issuing Xmit start request to BAL for avail res ASYNC"));
    return WDA_DS_StartXmit(pvosGCtx);
  }

  return VOS_STATUS_SUCCESS;
}/* WLANTL_ResourceCB */


/*==========================================================================
  FUNCTION   WLANTL_IsTxXmitPending

  DESCRIPTION
    Called by the WDA when it wants to know whether WDA_DS_TX_START_XMIT msg
    is pending in TL msg queue 

  DEPENDENCIES
    The TL must be registered with WDA before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    0:   No WDA_DS_TX_START_XMIT msg pending 
    1:   Msg WDA_DS_TX_START_XMIT already pending in TL msg queue 

  SIDE EFFECTS

============================================================================*/
v_BOOL_t
WLANTL_IsTxXmitPending
(
  v_PVOID_t       pvosGCtx
)
{

   WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
        "WLAN TL:Invalid TL pointer from pvosGCtx in WLANTL_IsTxXmitPending "));
    return FALSE;
  }

  return pTLCb->isTxTranmitMsgPending;

}/*WLANTL_IsTxXmitPending */

/*==========================================================================
  FUNCTION   WLANTL_SetTxXmitPending

  DESCRIPTION
    Called by the WDA when it wants to indicate that WDA_DS_TX_START_XMIT msg
    is pending in TL msg queue 

  DEPENDENCIES
    The TL must be registered with WDA before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context

  RETURN VALUE      None

  SIDE EFFECTS

============================================================================*/

v_VOID_t
WLANTL_SetTxXmitPending
(
  v_PVOID_t       pvosGCtx
)
{

   WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
        "WLAN TL:Invalid TL pointer from pvosGCtx in WLANTL_SetTxXmitPending"));
    return;
  }

  pTLCb->isTxTranmitMsgPending = 1;
  return;

}/*WLANTL_SetTxXmitPending  */

/*==========================================================================
  FUNCTION   WLANTL_ClearTxXmitPending

  DESCRIPTION
    Called by the WDA when it wants to indicate that no WDA_DS_TX_START_XMIT msg
    is pending in TL msg queue 

  DEPENDENCIES
    The TL must be registered with WDA before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context

  RETURN VALUE     None

  SIDE EFFECTS

============================================================================*/

v_VOID_t
WLANTL_ClearTxXmitPending
(
  v_PVOID_t       pvosGCtx
)
{

   WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
        "WLAN TL:Invalid TL pointer from pvosGCtx in WLANTL_ClearTxXmitPending "));
    return;
  }

  pTLCb->isTxTranmitMsgPending = 0;
  return;
}/*WLANTL_ClearTxXmitPending */

/*==========================================================================
  FUNCTION   WLANTL_TxThreadDebugHandler

  DESCRIPTION
    Printing TL Snapshot dump, processed under TxThread context, currently
    information regarding the global TlCb struture. Dumps information related
    to per active STA connection currently in use by TL.

  DEPENDENCIES
    The TL must be initialized before this gets called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context

  RETURN VALUE      None

  SIDE EFFECTS

============================================================================*/

v_VOID_t
WLANTL_TxThreadDebugHandler
(
 v_PVOID_t *pVosContext
)
{
   WLANTL_CbType* pTLCb = NULL;
   WLANTL_STAClientType* pClientSTA = NULL;
   int i = 0;
   v_U8_t uFlowMask; // TX FlowMask from WDA

   TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
        "WLAN TL: %s Enter ", __func__));

   pTLCb = VOS_GET_TL_CB(pVosContext);

   if ( NULL == pVosContext || NULL == pTLCb )
   {
        TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                        "Global VoS Context or TL Context are NULL"));
        return;
   }

   if (VOS_STATUS_SUCCESS == WDA_DS_GetTxFlowMask(pVosContext, &uFlowMask))
   {
        TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WDA uTxFlowMask: 0x%x", uFlowMask));
   }

   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "************************TL DUMP INFORMATION**************"));

   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "uDelayedTriggerFrmInt:%d\tuMinFramesProcThres:%d",
          pTLCb->tlConfigInfo.uDelayedTriggerFrmInt,
          pTLCb->tlConfigInfo.uMinFramesProcThres));

   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "Management Frame Client exists: %d",
          pTLCb->tlMgmtFrmClient.ucExists));
   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "usPendingTxCompleteCount: %d\tucTxSuspended: %d",
          pTLCb->usPendingTxCompleteCount,
          pTLCb->ucTxSuspended));

   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "uResCount: %d", pTLCb->uResCount));

   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "ucRegisteredStaId: %d\tucCurrentSTA: %d",
          pTLCb->ucRegisteredStaId, pTLCb->ucCurrentSTA));

   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "UrgentFrameProcessing: %s\tuFramesProcThres: %d",
          (pTLCb->bUrgent?"True":"False"), pTLCb->uFramesProcThres));

   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "isTxTranmitMsgPending: %d\t isBMPS: %s",
          pTLCb->isTxTranmitMsgPending, pTLCb->isBMPS?"True":"False"));

#ifdef FEATURE_WLAN_TDLS
   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "TDLS Peer Count: %d", pTLCb->ucTdlsPeerCount));
#endif

   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
          "++++++++++++++++++++Registerd Client Information++++++++++"));

   for ( i =0; i<WLAN_MAX_STA_COUNT; i++ )
   {
        pClientSTA = pTLCb->atlSTAClients[i];
        if( NULL == pClientSTA || 0 == pClientSTA->ucExists)
        {
                continue;
        }

        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "######################STA Index: %d ############################",i));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO, "WLAN_STADescType:"));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "STAId: %d\t STA MAC Address: %pM", pClientSTA->wSTADesc.ucSTAId,
              pClientSTA->wSTADesc.vSTAMACAddress.bytes));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "STA Type: %d\tProtectedFrame: %d",
              pClientSTA->wSTADesc.wSTAType, pClientSTA->wSTADesc.ucProtectedFrame));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
               "QoS: %d\tRxFrameTrans: %d\tTxFrameTrans: %d",
               pClientSTA->wSTADesc.ucQosEnabled, pClientSTA->wSTADesc.ucSwFrameRXXlation,
               pClientSTA->wSTADesc.ucSwFrameTXXlation));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "ucUcastSig: %d\tucBcastSig: %d", pClientSTA->wSTADesc.ucUcastSig,
              pClientSTA->wSTADesc.ucBcastSig));

        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "ClientIndex: %d\t Exists: %d", i, pClientSTA->ucExists));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "TL State: %d\t TL Priority: %d", pClientSTA->tlState,
              pClientSTA->tlPri));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "ucTxSuspended: %d\tucPktPending: %d", pClientSTA->ucTxSuspended,
              pClientSTA->ucPktPending));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "ucEAPOLPktPending: %d\tucNoMoreData: %d",
              pClientSTA->ucEapolPktPending, pClientSTA->ucNoMoreData));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
               "enableCaching: %d\t fcStaTxDisabled: %d", pClientSTA->enableCaching,
               pClientSTA->fcStaTxDisabled));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
               "ucCurrentAC: %d\tucServicedAC: %d", pClientSTA->ucCurrentAC,
               pClientSTA->ucServicedAC));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "TID: %d\tautTxCount[0]: %d\tauRxCount[0]: %d",0, pClientSTA->auTxCount[0],
              pClientSTA->auRxCount[0]));
        TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "aucAcMask[0]: %d\taucAcMask[1]: %d\taucAcMask[2]: %d\taucAcMask[3]: %d\t",
              pClientSTA->aucACMask[0], pClientSTA->aucACMask[1],
              pClientSTA->aucACMask[2], pClientSTA->aucACMask[3]));
        TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,

               "ucCurrentWeight: %d", pClientSTA->ucCurrentWeight));

        if( WLAN_STA_SOFTAP == pClientSTA->wSTADesc.wSTAType)
        {
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                  "TrafficStatistics for SOFTAP Station:"));
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                  "RUF=%d\tRMF=%d\tRBF=%d", pClientSTA->trafficStatistics.rxUCFcnt,
                                            pClientSTA->trafficStatistics.rxMCFcnt,
                                            pClientSTA->trafficStatistics.rxBCFcnt));
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                  "RUB=%d\tRMB=%d\tRBB=%d", pClientSTA->trafficStatistics.rxUCBcnt,
                                            pClientSTA->trafficStatistics.rxMCBcnt,
                                            pClientSTA->trafficStatistics.rxBCBcnt));
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                  "TUF=%d\tTMF=%d\tTBF=%d", pClientSTA->trafficStatistics.txUCFcnt,
                                            pClientSTA->trafficStatistics.txMCFcnt,
                                            pClientSTA->trafficStatistics.txBCFcnt));
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                  "TUB=%d\tTMB=%d\tTBB=%d", pClientSTA->trafficStatistics.txUCBcnt,
                                            pClientSTA->trafficStatistics.txMCBcnt,
                                            pClientSTA->trafficStatistics.txBCBcnt));
        }

    }
   return;
}

/*==========================================================================
  FUNCTION   WLANTL_FatalErrorHandler

  DESCRIPTION
    Handle Fatal errors detected on the TX path.
    Currently issues SSR to recover from the error.

  DEPENDENCIES
    The TL must be initialized before this gets called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    or WDA's control block can be extracted from its context

  RETURN VALUE      None

  SIDE EFFECTS

============================================================================*/
v_VOID_t
WLANTL_FatalErrorHandler
(
 v_PVOID_t *pVosContext
)
{

   TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
        "WLAN TL: %s Enter ", __func__));

   if ( NULL == pVosContext )
   {
        TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                        "%s: Global VoS Context or TL Context are NULL",
                        __func__));
        return;
   }

   /*
    * Issue SSR. vos_wlanRestart has tight checks to make sure that
    * we do not send an FIQ if previous FIQ is not processed
    */
   vos_wlanRestart();
}

/*==========================================================================
  FUNCTION   WLANTL_TLDebugMessage

  DESCRIPTION
    Post a TL Snapshot request, posts message in TxThread.

  DEPENDENCIES
    The TL must be initialized before this gets called.

  PARAMETERS

    IN
    displaySnapshot Boolean showing whether to dump the snapshot or not.

  RETURN VALUE      None

  SIDE EFFECTS

============================================================================*/

v_VOID_t
WLANTL_TLDebugMessage
(
 v_U32_t debugFlags
)
{
   vos_msg_t vosMsg;
   VOS_STATUS status;

   if(debugFlags & WLANTL_DEBUG_TX_SNAPSHOT)
   {
        vosMsg.reserved = 0;
        vosMsg.bodyptr  = NULL;
        vosMsg.type     = WLANTL_TX_SNAPSHOT;

        status = vos_tx_mq_serialize( VOS_MODULE_ID_TL, &vosMsg);
        if(status != VOS_STATUS_SUCCESS)
        {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR, "TX Msg Posting Failed with status: %d",status));
            return;
        }
   }
   if (debugFlags & WLANTL_DEBUG_FW_CLEANUP)
   {
        vosMsg.reserved = 0;
        vosMsg.bodyptr  = NULL;
        vosMsg.type     = WLANTL_TX_FW_DEBUG;

        status = vos_tx_mq_serialize( VOS_MODULE_ID_TL, &vosMsg);
        if(status != VOS_STATUS_SUCCESS)
        {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR, "TX Msg Posting Failed with status: %d",status));
            return;
        }
   }
   if(debugFlags & WLANTL_DEBUG_KICKDXE)
   {
        vosMsg.reserved = 0;
        vosMsg.bodyptr  = NULL;
        vosMsg.type     = WLANTL_TX_KICKDXE;

        status = vos_tx_mq_serialize( VOS_MODULE_ID_TL, &vosMsg);
        if(status != VOS_STATUS_SUCCESS)
        {
            TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR, "TX Msg Posting Failed with status: %d",status));
            return;
        }
   }
   return;
}

/*==========================================================================
  FUNCTION   WLANTL_FatalError

  DESCRIPTION
    Fatal error reported in TX path, post an event to TX Thread for further
    handling

  DEPENDENCIES
    The TL must be initialized before this gets called.

  PARAMETERS

    VOID

  RETURN VALUE      None

  SIDE EFFECTS

============================================================================*/

v_VOID_t
WLANTL_FatalError
(
 v_VOID_t
)
{
   vos_msg_t vosMsg;
   VOS_STATUS status;

   vosMsg.reserved = 0;
   vosMsg.bodyptr  = NULL;
   vosMsg.type     = WLANTL_TX_FATAL_ERROR;

   status = vos_tx_mq_serialize( VOS_MODULE_ID_TL, &vosMsg);
   if(status != VOS_STATUS_SUCCESS)
   {
       TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                        "%s: TX Msg Posting Failed with status: %d",
                        __func__,status));
   }
   return;
}
/*============================================================================
                           TL STATE MACHINE
============================================================================*/

/*==========================================================================
  FUNCTION    WLANTL_STATxConn

  DESCRIPTION
    Transmit in connected state - only EAPOL and WAI packets allowed

  DEPENDENCIES
    The STA must be registered with TL before this function can be called.

  PARAMETERS

   IN
   pvosGCtx:       pointer to the global vos context; a handle to TL's
                   control block can be extracted from its context
   ucSTAId:        identifier of the station being processed
   vosDataBuff:   pointer to the tx vos buffer

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

   Other return values are possible coming from the called functions.
   Please check API for additional info.

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_STATxConn
(
  v_PVOID_t     pvosGCtx,
  v_U8_t        ucSTAId,
  vos_pkt_t**   pvosDataBuff,
  v_BOOL_t      bForwardIAPPwithLLC
)
{
   v_U16_t              usPktLen;
   VOS_STATUS           vosStatus;
   v_MACADDR_t          vDestMacAddr;
   vos_pkt_t*           vosDataBuff = NULL;
   WLANTL_CbType*       pTLCb       = NULL;
   WLANTL_STAClientType* pClientSTA = NULL;
   WLANTL_MetaInfoType  tlMetaInfo;
   v_U8_t               ucTypeSubtype = 0;
   v_U8_t               ucTid;
   v_U8_t               extraHeadSpace = 0;
   v_U8_t               ucWDSEnabled = 0;
   v_U8_t               ucAC, ucACMask, i; 
   v_U32_t              txFlag = HAL_TX_NO_ENCRYPTION_MASK;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
   VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_STATxConn");
   *pvosDataBuff = NULL;
    return VOS_STATUS_E_FAULT;
  }
  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  /*-------------------------------------------------------------------
      Disable AC temporary - if successfull retrieve re-enable
      The order is justified because of the possible scenario
       - TL tryes to fetch packet for AC and it returns NULL
       - TL analyzes the data it has received to see if there are
       any more pkts available for AC -> if not TL will disable AC
       - however it is possible that while analyzing results TL got
       preempted by a pending indication where the mask was again set
       TL will not check again and as a result when it resumes
       execution it will disable AC
       To prevent this the AC will be disabled here and if retrieve
       is successfull it will be re-enabled
  -------------------------------------------------------------------*/


  //LTI:pTLCb->atlSTAClients[ucSTAId].
  //LTI:   aucACMask[pTLCb->atlSTAClients[ucSTAId].ucCurrentAC] = 0; 

  /*------------------------------------------------------------------------
    Fetch packet from HDD
   ------------------------------------------------------------------------*/
#ifdef FEATURE_WLAN_TDLS
  if ((WLAN_STA_SOFTAP != pClientSTA->wSTADesc.wSTAType) &&
      !(vos_concurrent_open_sessions_running()) &&
      !pTLCb->ucTdlsPeerCount)
  {
#else
  if ((WLAN_STA_SOFTAP != pClientSTA->wSTADesc.wSTAType) &&
      !(vos_concurrent_open_sessions_running()))
  {
#endif
      ucAC = pClientSTA->ucCurrentAC;

  /*-------------------------------------------------------------------
      Disable AC temporary - if successfull retrieve re-enable
      The order is justified because of the possible scenario
       - TL tryes to fetch packet for AC and it returns NULL
       - TL analyzes the data it has received to see if there are
       any more pkts available for AC -> if not TL will disable AC
       - however it is possible that while analyzing results TL got
       preempted by a pending indication where the mask was again set
       TL will not check again and as a result when it resumes
       execution it will disable AC
       To prevent this the AC will be disabled here and if retrieve
       is successfull it will be re-enabled
  -------------------------------------------------------------------*/
      pClientSTA->aucACMask[ucAC] = 0;
  }
  else
  {
      //softap case
      ucAC = pTLCb->uCurServedAC;
      pClientSTA->aucACMask[ucAC] = 0;
  }

    /*You make an initial assumption that HDD has no more data and if the
      assumption was wrong you reset the flags to their original state
     This will prevent from exposing a race condition between checking with HDD
     for packets and setting the flags to false*/
  //LTI: vos_atomic_set_U8( &pTLCb->atlSTAClients[ucSTAId].ucPktPending, 0);
  //LTI: pTLCb->atlSTAClients[ucSTAId].ucNoMoreData = 1;
  vos_atomic_set_U8( &pClientSTA->ucPktPending, 0);
  WLAN_TL_AC_ARRAY_2_MASK( pClientSTA, ucACMask, i);
    /*You make an initial assumption that HDD has no more data and if the
           assumption was wrong you reset the flags to their original state
           This will prevent from exposing a race condition between checking with HDD
           for packets and setting the flags to false*/
  if ( 0 == ucACMask )
  {
      pClientSTA->ucNoMoreData = 1;
  }
  else
  {
      vos_atomic_set_U8( &pClientSTA->ucPktPending, 1);
  }


  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
            "WLAN TL: WLANTL_STATxConn fetching packet from HDD for AC: %d AC Mask: %d Pkt Pending: %d", 
             ucAC, ucACMask, pClientSTA->ucPktPending);

  /*------------------------------------------------------------------------
    Fetch tx packet from HDD
   ------------------------------------------------------------------------*/

  vosStatus = pClientSTA->pfnSTAFetchPkt( pvosGCtx,
                               &ucSTAId,
                               ucAC,
                               &vosDataBuff, &tlMetaInfo );

  if (( VOS_STATUS_SUCCESS != vosStatus ) || ( NULL == vosDataBuff ))
  {
    TLLOG1(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
               "WLAN TL:No more data at HDD status %d", vosStatus));
    *pvosDataBuff = NULL;

    /*--------------------------------------------------------------------
    Reset AC for the serviced station to the highest priority AC
    -> due to no more data at the station
    Even if this AC is not supported by the station, correction will be
    made in the main TL loop
    --------------------------------------------------------------------*/
    pClientSTA->ucCurrentAC     = WLANTL_AC_HIGH_PRIO;
    pClientSTA->ucCurrentWeight = 0;

    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
              "WLAN TL: WLANTL_STATxConn no more packets in HDD for AC: %d AC Mask: %d", 
               ucAC, ucACMask);

    return vosStatus;
  }

  /*There are still packets in HDD - set back the pending packets and 
   the no more data assumption*/
  vos_atomic_set_U8( &pClientSTA->ucPktPending, 1);
  pClientSTA->ucNoMoreData = 0;
  pClientSTA->aucACMask[ucAC] = 1;

#ifdef WLAN_PERF 
  vos_pkt_set_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_BAL, 
                             (v_PVOID_t)0);

#endif /*WLAN_PERF*/


#ifdef FEATURE_WLAN_WAPI
   /*------------------------------------------------------------------------
    If the packet is neither an Eapol packet nor a WAI packet then drop it
   ------------------------------------------------------------------------*/
   if ( 0 == tlMetaInfo.ucIsEapol && 0 == tlMetaInfo.ucIsWai )
   {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                "WLAN TL:Only EAPOL or WAI packets allowed before authentication"));

     /* Fail tx for packet */
     pClientSTA->pfnSTATxComp( pvosGCtx, vosDataBuff,
                                                VOS_STATUS_E_BADMSG);
     vosDataBuff = NULL;
     *pvosDataBuff = NULL;
     return VOS_STATUS_SUCCESS;
  }
#else
   if ( 0 == tlMetaInfo.ucIsEapol )
  {
     TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                "WLAN TL:Received non EAPOL packet before authentication"));

    /* Fail tx for packet */
    pClientSTA->pfnSTATxComp( pvosGCtx, vosDataBuff,
                                                VOS_STATUS_E_BADMSG);
    vosDataBuff = NULL;
    *pvosDataBuff = NULL;
    return VOS_STATUS_SUCCESS;
  }
#endif /* FEATURE_WLAN_WAPI */

  /*-------------------------------------------------------------------------
   Check TID
  -------------------------------------------------------------------------*/
  ucTid     = tlMetaInfo.ucTID;

  /*Make sure TID is valid*/
  if ( WLANTL_TID_INVALID(ucTid)) 
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN TL:Invalid TID sent in meta info %d - defaulting to 0 (BE)", 
             ucTid));
     ucTid = 0; 
  }

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Attaching BD header to pkt on WLANTL_STATxConn"));

#ifdef FEATURE_WLAN_WAPI
  /*------------------------------------------------------------------------
    Translate 802.3 frame to 802.11 if Frame translation is enabled or if 
    frame is a WAI frame.
   ------------------------------------------------------------------------*/
  if ( ( 1 == tlMetaInfo.ucIsWai ) ||
       ( 0 == tlMetaInfo.ucDisableFrmXtl ) )
#else
  /*------------------------------------------------------------------------
    Translate 802.3 frame to 802.11 if Frame translation is enabled 
   ------------------------------------------------------------------------*/
  if ( ( 0 == tlMetaInfo.ucDisableFrmXtl ) &&
      ( 0 != pClientSTA->wSTADesc.ucSwFrameTXXlation) )
#endif //#ifdef FEATURE_WLAN_WAPI
  {
    vosStatus =  WLANTL_Translate8023To80211Header( vosDataBuff, &vosStatus,
                                                    pTLCb, &ucSTAId,
                                                    &tlMetaInfo, &ucWDSEnabled,
                                                    &extraHeadSpace);
    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:Error when translating header WLANTL_STATxConn"));

      return vosStatus;
    }

    tlMetaInfo.ucDisableFrmXtl = 1;
  }

  /*-------------------------------------------------------------------------
    Call HAL to fill BD header
   -------------------------------------------------------------------------*/
  ucTypeSubtype |= (WLANTL_80211_DATA_TYPE << 4);

  if ( pClientSTA->wSTADesc.ucQosEnabled )
  {
    ucTypeSubtype |= (WLANTL_80211_DATA_QOS_SUBTYPE);
  }

#ifdef FEATURE_WLAN_WAPI
  /* TL State does not transition to AUTHENTICATED till GTK is installed, So in
   * case of WPA where GTK handshake is done after the 4 way handshake, the
   * unicast 2/2 EAPOL packet from the STA->AP has to be encrypted even before
   * the TL is in authenticated state. Since the PTK has been installed
   * already (after the 4 way handshake) we make sure that all traffic
   * is encrypted henceforth.(Note: TL is still not in AUTHENTICATED state so
   * we will only allow EAPOL data or WAI in case of WAPI)
   */
  if (tlMetaInfo.ucIsEapol && pClientSTA->ptkInstalled)
  {
    txFlag = 0;
  }
#else
  if (pClientSTA->ptkInstalled)
  {
    txFlag = 0;
  }
#endif

  vosStatus = (VOS_STATUS)WDA_DS_BuildTxPacketInfo( pvosGCtx, vosDataBuff , &vDestMacAddr,
                          tlMetaInfo.ucDisableFrmXtl, &usPktLen,
                          pClientSTA->wSTADesc.ucQosEnabled, ucWDSEnabled,
                          extraHeadSpace,
                          ucTypeSubtype, &pClientSTA->wSTADesc.vSelfMACAddress,
                          ucTid, txFlag,
                          tlMetaInfo.usTimeStamp, tlMetaInfo.ucIsEapol || tlMetaInfo.ucIsWai, tlMetaInfo.ucUP,
                          tlMetaInfo.ucTxBdToken);

  if ( VOS_STATUS_SUCCESS != vosStatus )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:Failed while attempting to fill BD %d", vosStatus));
    *pvosDataBuff = NULL;
    return vosStatus;
  }

  /*-----------------------------------------------------------------------
    Update tx counter for BA session query for tx side
    !1 - should this be done for EAPOL frames?
    -----------------------------------------------------------------------*/
  pClientSTA->auTxCount[ucTid]++;

  vos_pkt_set_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_TL,
               (v_PVOID_t)pClientSTA->pfnSTATxComp );

  /*------------------------------------------------------------------------
    Save data to input pointer for TL core
  ------------------------------------------------------------------------*/
  *pvosDataBuff = vosDataBuff;
  /*security frames cannot be delayed*/
  pTLCb->bUrgent      = TRUE;

  /* TX Statistics */
  if (!(tlMetaInfo.ucBcast || tlMetaInfo.ucMcast))
  {
    /* This is TX UC frame */
    pClientSTA->trafficStatistics.txUCFcnt++;
    pClientSTA->trafficStatistics.txUCBcnt += usPktLen;
  }

  return VOS_STATUS_SUCCESS;
}/* WLANTL_STATxConn */


/*==========================================================================
  FUNCTION    WLANTL_STATxAuth

  DESCRIPTION
    Transmit in authenticated state - all data allowed

  DEPENDENCIES
    The STA must be registered with TL before this function can be called.

  PARAMETERS

   IN
   pvosGCtx:       pointer to the global vos context; a handle to TL's
                   control block can be extracted from its context
   ucSTAId:        identifier of the station being processed
   vosDataBuff:   pointer to the tx vos buffer

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

   Other return values are possible coming from the called functions.
   Please check API for additional info.

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_STATxAuth
(
  v_PVOID_t     pvosGCtx,
  v_U8_t        ucSTAId,
  vos_pkt_t**   pvosDataBuff,
  v_BOOL_t      bForwardIAPPwithLLC
)
{
   v_U16_t               usPktLen;
   VOS_STATUS            vosStatus;
   v_MACADDR_t           vDestMacAddr;
   vos_pkt_t*            vosDataBuff = NULL;
   WLANTL_CbType*        pTLCb       = NULL;
   WLANTL_MetaInfoType   tlMetaInfo;
   v_U8_t                ucTypeSubtype = 0;
   WLANTL_ACEnumType     ucAC;
   WLANTL_ACEnumType     ucNextAC;
   v_U8_t                ucTid;
   v_U8_t                ucSwFrmXtl = 0;
   v_U8_t                extraHeadSpace = 0;
   WLANTL_STAClientType *pStaClient = NULL;
   v_U8_t                ucWDSEnabled = 0;
   v_U32_t               ucTxFlag   = 0;
   v_U8_t                ucACMask, i;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if (( NULL == pTLCb ) || ( NULL == pvosDataBuff ))
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Invalid input params on WLANTL_STATxAuth TL %p DB %p",
             pTLCb, pvosDataBuff));
    if (NULL != pvosDataBuff)
    {
        *pvosDataBuff = NULL;
    }
    if(NULL != pTLCb)
    {
        if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
        {
            TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:Client Memory was not allocated on %s", __func__));
            return VOS_STATUS_E_FAILURE;
        }
        pTLCb->atlSTAClients[ucSTAId]->ucNoMoreData = 1;
    }
    return VOS_STATUS_E_FAULT;
  }
  pStaClient = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pStaClient )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  vos_mem_zero(&tlMetaInfo, sizeof(tlMetaInfo));
  /*------------------------------------------------------------------------
    Fetch packet from HDD
   ------------------------------------------------------------------------*/
#ifdef FEATURE_WLAN_TDLS
  if ((WLAN_STA_SOFTAP != pStaClient->wSTADesc.wSTAType) &&
      (!vos_concurrent_open_sessions_running()) &&
      !pTLCb->ucTdlsPeerCount)
  {
#else
  if ((WLAN_STA_SOFTAP != pStaClient->wSTADesc.wSTAType) &&
      (!vos_concurrent_open_sessions_running()))
  {
#endif
  ucAC = pStaClient->ucCurrentAC;

  /*-------------------------------------------------------------------
      Disable AC temporary - if successfull retrieve re-enable
      The order is justified because of the possible scenario
       - TL tryes to fetch packet for AC and it returns NULL
       - TL analyzes the data it has received to see if there are
       any more pkts available for AC -> if not TL will disable AC
       - however it is possible that while analyzing results TL got
       preempted by a pending indication where the mask was again set
       TL will not check again and as a result when it resumes
       execution it will disable AC
       To prevent this the AC will be disabled here and if retrieve
       is successfull it will be re-enabled
  -------------------------------------------------------------------*/
  pStaClient->aucACMask[pStaClient->ucCurrentAC] = 0; 

  // don't reset it, as other AC queues in HDD may have packets
  //vos_atomic_set_U8( &pStaClient->ucPktPending, 0);
  }
  else
  {
    //softap case
    ucAC = pTLCb->uCurServedAC;
    pStaClient->aucACMask[ucAC] = 0; 

    //vos_atomic_set_U8( &pStaClient->ucPktPending, 0);
  }

  WLAN_TL_AC_ARRAY_2_MASK( pStaClient, ucACMask, i); 
    /*You make an initial assumption that HDD has no more data and if the 
      assumption was wrong you reset the flags to their original state
     This will prevent from exposing a race condition between checking with HDD 
     for packets and setting the flags to false*/
  if ( 0 == ucACMask )
  {
    vos_atomic_set_U8( &pStaClient->ucPktPending, 0);
    pStaClient->ucNoMoreData = 1;
  }

  vosStatus = pStaClient->pfnSTAFetchPkt( pvosGCtx, 
                               &ucSTAId,
                               ucAC,
                               &vosDataBuff, &tlMetaInfo );


  if (( VOS_STATUS_SUCCESS != vosStatus ) || ( NULL == vosDataBuff ))
  {

    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
               "WLAN TL:Failed while attempting to fetch pkt from HDD QId:%d status:%d",
               ucAC, vosStatus);
    *pvosDataBuff = NULL;
    /*--------------------------------------------------------------------
      Reset AC for the serviced station to the highest priority AC
      -> due to no more data at the station
      Even if this AC is not supported by the station, correction will be
      made in the main TL loop
    --------------------------------------------------------------------*/
    pStaClient->ucCurrentAC     = WLANTL_AC_HIGH_PRIO;
    pStaClient->ucCurrentWeight = 0;

    return vosStatus;
  }

  WLANTL_StatHandleTXFrame(pvosGCtx, ucSTAId, vosDataBuff, NULL, &tlMetaInfo);

  /*There are still packets in HDD - set back the pending packets and 
   the no more data assumption*/
  vos_atomic_set_U8( &pStaClient->ucPktPending, 1);
  pStaClient->ucNoMoreData = 0;

  if (WLAN_STA_SOFTAP != pStaClient->wSTADesc.wSTAType)
  {
  // don't need to set it, as we don't reset it in this function.
  //vos_atomic_set_U8( &pTLCb->atlSTAClients[ucSTAId].ucPktPending, 1);
  }

#ifdef WLAN_PERF 
   vos_pkt_set_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_BAL, 
                       (v_PVOID_t)0);
#endif /*WLAN_PERF*/

   /*-------------------------------------------------------------------------
    Check TID
   -------------------------------------------------------------------------*/
   ucTid     = tlMetaInfo.ucTID;

  /*Make sure TID is valid*/
  if ( WLANTL_TID_INVALID(ucTid)) 
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN TL:Invalid TID sent in meta info %d - defaulting to 0 (BE)", 
             ucTid));
     ucTid = 0; 
  }

  /*Save for UAPSD timer consideration*/
  pStaClient->ucServicedAC = ucAC; 

  if ( ucAC == pStaClient->ucCurrentAC ) 
  {
    pStaClient->aucACMask[pStaClient->ucCurrentAC] = 1;
    pStaClient->ucCurrentWeight--;
  }
  else
  {
    pStaClient->ucCurrentAC     = ucAC;
    pStaClient->ucCurrentWeight = pTLCb->tlConfigInfo.ucAcWeights[ucAC] - 1;

    pStaClient->aucACMask[pStaClient->ucCurrentAC] = 1;

  }

  if (WLAN_STA_SOFTAP != pStaClient->wSTADesc.wSTAType)
  {
  if ( 0 == pStaClient->ucCurrentWeight ) 
  {
    WLANTL_ACEnumType tempAC = ucAC;
    /*-----------------------------------------------------------------------
       Choose next AC - !!! optimize me
    -----------------------------------------------------------------------*/
    while ( 0 != ucACMask ) 
    {
      if(tempAC == WLANTL_AC_BK)
         ucNextAC = WLANTL_AC_HIGH_PRIO;
      else
         ucNextAC = (tempAC - 1);

      if ( 0 != pStaClient->aucACMask[ucNextAC] )
      {
         pStaClient->ucCurrentAC     = ucNextAC;
         pStaClient->ucCurrentWeight = pTLCb->tlConfigInfo.ucAcWeights[ucNextAC];

         TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                    "WLAN TL: Changing serviced AC to: %d with Weight: %d",
                    pStaClient->ucCurrentAC , 
                    pStaClient->ucCurrentWeight));
         break;
      }
      tempAC = ucNextAC;
    }
  }
  }

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Attaching BD header to pkt on WLANTL_STATxAuth"));

  /*------------------------------------------------------------------------
    Translate 802.3 frame to 802.11
   ------------------------------------------------------------------------*/
  if ( 0 == tlMetaInfo.ucDisableFrmXtl )
  {
     /* Needs frame translation */
     // if the client has not enabled SW-only frame translation
     // and if the frame is a unicast frame
     //   (HW frame translation does not support multiple broadcast domains
     //    so we use SW frame translation for broadcast/multicast frames)
#ifdef FEATURE_WLAN_WAPI
     // and if the frame is not a WAPI frame
#endif
     // then use HW_based frame translation

     if ( ( 0 == pStaClient->wSTADesc.ucSwFrameTXXlation ) &&
          ( 0 == tlMetaInfo.ucBcast ) &&
          ( 0 == tlMetaInfo.ucMcast )
#ifdef FEATURE_WLAN_WAPI
          && ( tlMetaInfo.ucIsWai != 1 )
#endif
        )
     {
#ifdef WLAN_PERF 
        v_U32_t uFastFwdOK = 0;

        /* HW based translation. See if the frame could be fast forwarded */
        WDA_TLI_FastHwFwdDataFrame( pvosGCtx, vosDataBuff , &vosStatus, 
                                   &uFastFwdOK, &tlMetaInfo, &pStaClient->wSTADesc);

        if( VOS_STATUS_SUCCESS == vosStatus )
        {
            if(uFastFwdOK)
            {
                /* Packet could be fast forwarded now */
                vos_pkt_set_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_TL, 
                               (v_PVOID_t)pStaClient->pfnSTATxComp );

                *pvosDataBuff = vosDataBuff;

                /* TODO: Do we really need to update WLANTL_HSHandleTXFrame() 
                   stats for every pkt? */
                pStaClient->auTxCount[tlMetaInfo.ucTID]++;
                return vosStatus;
             }
             /* can't be fast forwarded, fall through normal (slow) path. */
        }
        else
        {

            TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                       "WLAN TL:Failed while attempting to fastFwd BD %d", vosStatus));
            *pvosDataBuff = NULL;
            return vosStatus;
        }
#endif /*WLAN_PERF*/
     }
     else
     {
        /* SW based translation */

       vosStatus =  WLANTL_Translate8023To80211Header( vosDataBuff, &vosStatus,
                                                    pTLCb, &ucSTAId,
                                                    &tlMetaInfo, &ucWDSEnabled,
                                                    &extraHeadSpace);
       if ( VOS_STATUS_SUCCESS != vosStatus )
       {
          TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                    "WLAN TL:Error when translating header WLANTL_STATxAuth"));
          return vosStatus;
       }

       TLLOG4(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
                    "WLAN TL software translation success"));
       ucSwFrmXtl = 1;
       tlMetaInfo.ucDisableFrmXtl = 1;
    }
  }
#ifdef FEATURE_WLAN_TDLS
    /*In case of TDLS, if the packet is destined to TDLS STA ucSTAId may
      change. so update the pStaClient accordingly */
    pStaClient = pTLCb->atlSTAClients[ucSTAId];

    if ( NULL == pStaClient )
    {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "pStaClient is NULL %s", __func__));
        return VOS_STATUS_E_FAILURE;
    }
#endif
  /*-------------------------------------------------------------------------
    Call HAL to fill BD header
   -------------------------------------------------------------------------*/
  ucTypeSubtype |= (WLANTL_80211_DATA_TYPE << 4);

  if ( pStaClient->wSTADesc.ucQosEnabled ) 
  {
    ucTypeSubtype |= (WLANTL_80211_DATA_QOS_SUBTYPE);
  }

  /* ucAC now points to TL Q ID with a new queue added in TL,
   * hence look for the uapsd info for the correct AC that
   * this packet belongs to.
   */
  ucTxFlag  = (0 != pStaClient->wUAPSDInfo[tlMetaInfo.ac].ucSet)?
              HAL_TRIGGER_ENABLED_AC_MASK:0;

#ifdef FEATURE_WLAN_WAPI
  if ( pStaClient->wSTADesc.ucIsWapiSta == 1 )
  {
#ifdef LIBRA_WAPI_SUPPORT
    ucTxFlag = ucTxFlag | HAL_WAPI_STA_MASK;
#endif //LIBRA_WAPI_SUPPORT
    if ( tlMetaInfo.ucIsWai == 1 ) 
    {
      ucTxFlag = ucTxFlag | HAL_TX_NO_ENCRYPTION_MASK;
    }
  }
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_TDLS
  if ( pStaClient->wSTADesc.wSTAType == WLAN_STA_TDLS )
  {
    ucTxFlag = ucTxFlag | HAL_TDLS_PEER_STA_MASK;
  }
#endif /* FEATURE_WLAN_TDLS */
  if( tlMetaInfo.ucIsArp )
  {
    if (pStaClient->arpOnWQ5)
    {
        ucTxFlag |= HAL_USE_FW_IN_TX_PATH;
    }
    if (pStaClient->arpRate == 0)
    {
        ucTxFlag |= HAL_USE_BD_RATE_1_MASK;
    }
    else if (pStaClient->arpRate == 1 || pStaClient->arpRate == 3)
    {
        pStaClient->arpRate ^= 0x2;
        ucTxFlag |= HAL_USE_BD_RATE_1_MASK<<(pStaClient->arpRate-1);
    }
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
           "arp pkt sending on BD rate: %hhu", pStaClient->arpRate));
  }

  vosStatus = (VOS_STATUS)WDA_DS_BuildTxPacketInfo( pvosGCtx, 
                     vosDataBuff , &vDestMacAddr,
                     tlMetaInfo.ucDisableFrmXtl, &usPktLen,
                     pStaClient->wSTADesc.ucQosEnabled, ucWDSEnabled, 
                     extraHeadSpace,
                     ucTypeSubtype, &pStaClient->wSTADesc.vSelfMACAddress,
                     ucTid, ucTxFlag, tlMetaInfo.usTimeStamp, 
                     tlMetaInfo.ucIsEapol, tlMetaInfo.ucUP,
                     tlMetaInfo.ucTxBdToken);

  if(!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "Fill TX BD Error status %d", vosStatus));

    return vosStatus;
  }

  /* TX Statistics */
  if (!(tlMetaInfo.ucBcast || tlMetaInfo.ucMcast))
  {
    /* This is TX UC frame */
    pStaClient->trafficStatistics.txUCFcnt++;
    pStaClient->trafficStatistics.txUCBcnt += usPktLen;
  }

#ifndef FEATURE_WLAN_TDLS
  /*-----------------------------------------------------------------------
    Update tx counter for BA session query for tx side
    -----------------------------------------------------------------------*/
  pStaClient->auTxCount[ucTid]++;
#else
  pTLCb->atlSTAClients[ucSTAId]->auTxCount[ucTid]++;
#endif

  /* This code is to send traffic with lower priority AC when we does not 
     get admitted to send it. Today HAL does not downgrade AC so this code 
     does not get executed.(In other words, HAL doesn’t change tid. The if 
     statement is always false.)
     NOTE: In the case of LA downgrade occurs in HDD (that was the change 
     Phani made during WMM-AC plugfest). If WM & BMP also took this approach, 
     then there will be no need for any AC downgrade logic in TL/WDI.   */
#if 0
  if (( ucTid != tlMetaInfo.ucTID ) &&
      ( 0 != pStaClient->wSTADesc.ucQosEnabled ) && 
      ( 0 != ucSwFrmXtl ))
  {
    /*---------------------------------------------------------------------
      !! FIX me: Once downgrading is clear put in the proper change
    ---------------------------------------------------------------------*/
    ucQCOffset = WLANHAL_TX_BD_HEADER_SIZE + WLANTL_802_11_HEADER_LEN;

    //!!!Fix this replace peek with extract 
    vos_pkt_peek_data( vosDataBuff, ucQCOffset,(v_PVOID_t)&pucQosCtrl,
                       sizeof(*pucQosCtrl));
    *pucQosCtrl = ucTid; //? proper byte order
  }
#endif

  if ( VOS_STATUS_SUCCESS != vosStatus )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Failed while attempting to fill BD %d", vosStatus));
    *pvosDataBuff = NULL;
    return vosStatus;
  }

  vos_pkt_set_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_TL,
                   (v_PVOID_t)pStaClient->pfnSTATxComp );

  *pvosDataBuff = vosDataBuff;

  /*BE & BK can be delayed, VO and VI not frames cannot be delayed*/
  if ( pStaClient->ucServicedAC > WLANTL_AC_BE ) 
  {
    pTLCb->bUrgent= TRUE;
  }

  return VOS_STATUS_SUCCESS;
}/* WLANTL_STATxAuth */

/*==========================================================================
  FUNCTION    WLANTL_STATxDisc

  DESCRIPTION
    Transmit in disconnected state - no data allowed

  DEPENDENCIES
    The STA must be registered with TL before this function can be called.

  PARAMETERS

   IN
   pvosGCtx:       pointer to the global vos context; a handle to TL's
                   control block can be extracted from its context
   ucSTAId:        identifier of the station being processed
   vosDataBuff:   pointer to the tx vos buffer

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_STATxDisc
(
  v_PVOID_t     pvosGCtx,
  v_U8_t        ucSTAId,
  vos_pkt_t**   pvosDataBuff,
  v_BOOL_t      bForwardIAPPwithLLC
)
{
   WLANTL_CbType*        pTLCb       = NULL;
   WLANTL_STAClientType* pClientSTA = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_STATxAuth"));
    *pvosDataBuff = NULL;
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  /*------------------------------------------------------------------------
    Error
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
    "WLAN TL:Packet should not be transmitted in state disconnected ignoring"
            " request"));

  *pvosDataBuff = NULL;
   pClientSTA->ucNoMoreData = 1;
   
   //Should not be anything pending in disconnect state
   vos_atomic_set_U8( &pClientSTA->ucPktPending, 0);

  return VOS_STATUS_SUCCESS;
}/* WLANTL_STATxDisc */

/*==========================================================================
  FUNCTION    WLANTL_STARxConn

  DESCRIPTION
    Receive in connected state - only EAPOL

  DEPENDENCIES
    The STA must be registered with TL before this function can be called.

  PARAMETERS

   IN
   pvosGCtx:       pointer to the global vos context; a handle to TL's
                   control block can be extracted from its context
   ucSTAId:        identifier of the station being processed
   vosDataBuff:   pointer to the tx/rx vos buffer

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_STARxConn
(
  v_PVOID_t     pvosGCtx,
  v_U8_t        ucSTAId,
  vos_pkt_t**   pvosDataBuff,
  v_BOOL_t      bForwardIAPPwithLLC
)
{
   WLANTL_CbType*           pTLCb = NULL;
   WLANTL_STAClientType*    pClientSTA = NULL;
   v_U16_t                  usEtherType = 0;
   v_U16_t                  usPktLen;
   v_U8_t                   ucMPDUHOffset;
   v_U16_t                  usMPDUDOffset;
   v_U16_t                  usMPDULen;
   v_U8_t                   ucMPDUHLen;
   v_U16_t                  usActualHLen = 0;
   VOS_STATUS               vosStatus  = VOS_STATUS_SUCCESS;
   vos_pkt_t*               vosDataBuff;
   v_PVOID_t                aucBDHeader;
   v_U8_t                   ucTid;
   WLANTL_RxMetaInfoType    wRxMetaInfo;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if (( NULL == pvosDataBuff ) || ( NULL == ( vosDataBuff = *pvosDataBuff )))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_STARxConn"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ChangeSTAState"));
    return VOS_STATUS_E_FAULT;
  }
  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  /*------------------------------------------------------------------------
    Extract BD header and check if valid
   ------------------------------------------------------------------------*/
  vosStatus = WDA_DS_PeekRxPacketInfo( vosDataBuff, (v_PVOID_t)&aucBDHeader, 0/*Swap BD*/ );

  if ( NULL == aucBDHeader )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Cannot extract BD header"));
    VOS_ASSERT( 0 );
    return VOS_STATUS_E_FAULT;
  }


  ucMPDUHOffset = (v_U8_t)WDA_GET_RX_MPDU_HEADER_OFFSET(aucBDHeader);
  usMPDUDOffset = (v_U16_t)WDA_GET_RX_MPDU_DATA_OFFSET(aucBDHeader);
  usMPDULen     = (v_U16_t)WDA_GET_RX_MPDU_LEN(aucBDHeader);
  ucMPDUHLen    = (v_U8_t)WDA_GET_RX_MPDU_HEADER_LEN(aucBDHeader);
  ucTid         = (v_U8_t)WDA_GET_RX_TID(aucBDHeader);

  vos_pkt_get_packet_length( vosDataBuff, &usPktLen);

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:BD header processing data: HO %d DO %d Len %d HLen %d",
             ucMPDUHOffset, usMPDUDOffset, usMPDULen, ucMPDUHLen));

    /*It will cut out the 802.11 header if not used*/
  if ( VOS_STATUS_SUCCESS != WDA_DS_TrimRxPacketInfo( vosDataBuff ) )
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:BD header corrupted - dropping packet"));
    /* Drop packet */
    vos_pkt_return_packet(vosDataBuff);
    return VOS_STATUS_SUCCESS;
  }

  vosStatus = WLANTL_GetEtherType(aucBDHeader,vosDataBuff,ucMPDUHLen,&usEtherType);
  
  if( VOS_IS_STATUS_SUCCESS(vosStatus) )
  {
#ifdef FEATURE_WLAN_WAPI
    /* If frame is neither an EAPOL frame nor a WAI frame then we drop the frame*/
    /* TODO: Do we need a check to see if we are in WAPI mode? If not is it possible */
    /* that we get an EAPOL packet in WAPI mode or vice versa? */
    if ( WLANTL_LLC_8021X_TYPE  != usEtherType && WLANTL_LLC_WAI_TYPE  != usEtherType )
    {
      VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                 "WLAN TL:RX Frame not EAPOL or WAI EtherType %d - dropping", usEtherType );
      /* Drop packet */
      vos_pkt_return_packet(vosDataBuff);
    }
#else
    if ( WLANTL_LLC_8021X_TYPE  != usEtherType )
    {
      VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "WLAN TL:RX Frame not EAPOL EtherType %d - dropping", usEtherType);
      /* Drop packet */
      vos_pkt_return_packet(vosDataBuff);
    }
#endif /* FEATURE_WLAN_WAPI */
    else /* Frame is an EAPOL frame or a WAI frame*/  
    {
      MTRACE(vos_trace(VOS_MODULE_ID_TL,
                   TRACE_CODE_TL_RX_CONN_EAPOL, ucSTAId, usEtherType ));

      VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
                 "WLAN TL:RX Frame  EAPOL EtherType %d - processing", usEtherType);

      if (( 0 == WDA_GET_RX_FT_DONE(aucBDHeader) ) &&
         ( 0 != pClientSTA->wSTADesc.ucSwFrameRXXlation))
      {
      if (usMPDUDOffset > ucMPDUHOffset)
      {
         usActualHLen = usMPDUDOffset - ucMPDUHOffset;
      }

      vosStatus = WLANTL_Translate80211To8023Header( vosDataBuff, &vosStatus, usActualHLen, 
                      ucMPDUHLen, pTLCb, ucSTAId, bForwardIAPPwithLLC);

        if ( VOS_STATUS_SUCCESS != vosStatus ) 
        {
          TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
            "WLAN TL:Failed to translate from 802.11 to 802.3 - dropping"));
          /* Drop packet */
          vos_pkt_return_packet(vosDataBuff);
          return vosStatus;
        }
      }
      /*-------------------------------------------------------------------
      Increment receive counter
      -------------------------------------------------------------------*/
      if ( !WLANTL_TID_INVALID( ucTid) ) 
      {
        pClientSTA->auRxCount[ucTid]++;
      }
      else
      {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid tid  %d (Station ID %d) on %s",
               ucTid, ucSTAId, __func__));
      }

      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:Sending EAPoL frame to station %d AC %d", ucSTAId, ucTid));

      /*-------------------------------------------------------------------
      !!!Assuming TID = UP mapping 
      -------------------------------------------------------------------*/
      wRxMetaInfo.ucUP = ucTid;

      TLLOG4(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
               "WLAN TL %s:Sending data chain to station", __func__));
      if ( WLAN_STA_SOFTAP == pClientSTA->wSTADesc.wSTAType )
      {
        wRxMetaInfo.ucDesSTAId = WLAN_RX_SAP_SELF_STA_ID;
        pClientSTA->pfnSTARx( pvosGCtx, vosDataBuff, ucSTAId,
                                            &wRxMetaInfo );
      }
      else
      pClientSTA->pfnSTARx( pvosGCtx, vosDataBuff, ucSTAId,
                                            &wRxMetaInfo );
    }/*EAPOL frame or WAI frame*/
  }/*vos status success*/

  return VOS_STATUS_SUCCESS;
}/* WLANTL_STARxConn */

/*==========================================================================
  FUNCTION    WLANTL_FwdPktToHDD

  DESCRIPTION
    Determine the Destation Station ID and route the Frame to Upper Layer

  DEPENDENCIES

  PARAMETERS

   IN
   pvosGCtx:       pointer to the global vos context; a handle to TL's
                   control block can be extracted from its context
   ucSTAId:        identifier of the station being processed
   vosDataBuff:   pointer to the rx vos buffer

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/

VOS_STATUS
WLANTL_FwdPktToHDD
(
  v_PVOID_t       pvosGCtx,
  vos_pkt_t*     pvosDataBuff,
  v_U8_t          ucSTAId
)
{
   v_MACADDR_t              DestMacAddress;
   v_MACADDR_t              *pDestMacAddress = &DestMacAddress;
   v_SIZE_t                 usMacAddSize = VOS_MAC_ADDR_SIZE;
   WLANTL_CbType*           pTLCb = NULL;
   WLANTL_STAClientType*    pClientSTA = NULL;
   vos_pkt_t*               vosDataBuff ;
   VOS_STATUS               vosStatus = VOS_STATUS_SUCCESS;
   v_U32_t*                 STAMetaInfoPtr;
   vos_pkt_t*               vosNextDataBuff ;
   v_U8_t                   ucDesSTAId;
   WLANTL_RxMetaInfoType    wRxMetaInfo;

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if (( NULL == pvosDataBuff ) || ( NULL == ( vosDataBuff = pvosDataBuff )))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid parameter sent on WLANTL_FwdPktToHdd"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_FwdPktToHdd"));
    return VOS_STATUS_E_FAULT;
  }

  if(WLANTL_STA_ID_INVALID(ucSTAId))
  {
     TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"ucSTAId %d is not valid",
                 ucSTAId));
     return VOS_STATUS_E_INVAL;
  }

  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

   /* This the change required for SoftAp to handle Reordered Buffer. Since a STA
      may have packets destined to multiple destinations we have to process each packet
      at a time and determine its Destination. So the Voschain provided by Reorder code
      is unchain and forwarded to Upper Layer after Determining the Destination */

   vosDataBuff = pvosDataBuff;
   while (vosDataBuff != NULL)
   {
      vos_pkt_walk_packet_chain( vosDataBuff, &vosNextDataBuff, 1/*true*/ );
      vos_pkt_get_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_TL,
                                 (v_PVOID_t *)&STAMetaInfoPtr );
      wRxMetaInfo.ucUP = (v_U8_t)((uintptr_t)STAMetaInfoPtr & WLANTL_AC_MASK);
      ucDesSTAId = (v_U8_t)(((uintptr_t)STAMetaInfoPtr) >> WLANTL_STAID_OFFSET);
       
      vosStatus = vos_pkt_extract_data( vosDataBuff, 0, (v_VOID_t *)pDestMacAddress, &usMacAddSize);
      if ( VOS_STATUS_SUCCESS != vosStatus )
      {
         TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL: recv corrupted data packet"));
         vos_pkt_return_packet(vosDataBuff);
         return vosStatus;
      }

      TLLOG4(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
                        "station mac "MAC_ADDRESS_STR,
                        MAC_ADDR_ARRAY(pDestMacAddress->bytes)));

      if (vos_is_macaddr_broadcast( pDestMacAddress ) || vos_is_macaddr_group(pDestMacAddress))
      {
          // destination is mc/bc station
          ucDesSTAId = WLAN_RX_BCMC_STA_ID;
          TLLOG4(VOS_TRACE( VOS_MODULE_ID_HDD_SOFTAP, VOS_TRACE_LEVEL_INFO_LOW,
                    "%s: BC/MC packet, id %d", __func__, WLAN_RX_BCMC_STA_ID));
      }
      else
      {
         if (vos_is_macaddr_equal(pDestMacAddress, &pClientSTA->wSTADesc.vSelfMACAddress))
         {
            // destination is AP itself
            ucDesSTAId = WLAN_RX_SAP_SELF_STA_ID;
            TLLOG4(VOS_TRACE( VOS_MODULE_ID_HDD_SOFTAP, VOS_TRACE_LEVEL_INFO_LOW,
                     "%s: packet to AP itself, id %d", __func__, WLAN_RX_SAP_SELF_STA_ID));
         }
         else if (( WLAN_MAX_STA_COUNT <= ucDesSTAId ) || (NULL != pTLCb->atlSTAClients[ucDesSTAId] && pTLCb->atlSTAClients[ucDesSTAId]->ucExists == 0))
         {
            // destination station is something else
            TLLOG4(VOS_TRACE( VOS_MODULE_ID_HDD_SOFTAP, VOS_TRACE_LEVEL_INFO_LOW,
                 "%s: get an station index larger than WLAN_MAX_STA_COUNT %d", __func__, ucDesSTAId));
            ucDesSTAId = WLAN_RX_SAP_SELF_STA_ID;
         }

         
         //loopback unicast station comes here
      }

      wRxMetaInfo.ucUP = (v_U8_t)((uintptr_t)STAMetaInfoPtr & WLANTL_AC_MASK);
      wRxMetaInfo.ucDesSTAId = ucDesSTAId;
     
      vosStatus = pClientSTA->pfnSTARx( pvosGCtx, vosDataBuff, ucDesSTAId,
                                            &wRxMetaInfo );
      if ( VOS_STATUS_SUCCESS != vosStatus )
      {
          TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL: failed to send pkt to HDD"));
          vos_pkt_return_packet(vosDataBuff);

          return vosStatus;
      }
      vosDataBuff = vosNextDataBuff;
   }
   return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANTL_STARxAuth

  DESCRIPTION
    Receive in authenticated state - all data allowed

  DEPENDENCIES
    The STA must be registered with TL before this function can be called.

  PARAMETERS

   IN
   pvosGCtx:       pointer to the global vos context; a handle to TL's
                   control block can be extracted from its context
   ucSTAId:        identifier of the station being processed
   vosDataBuff:   pointer to the rx vos buffer

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_STARxAuth
(
  v_PVOID_t     pvosGCtx,
  v_U8_t        ucSTAId,
  vos_pkt_t**   pvosDataBuff,
  v_BOOL_t      bForwardIAPPwithLLC
)
{
   WLANTL_CbType*           pTLCb = NULL;
   WLANTL_STAClientType*    pClientSTA = NULL;
   v_U8_t                   ucAsf; /* AMSDU sub frame */
   v_U16_t                  usMPDUDOffset;
   v_U8_t                   ucMPDUHOffset;
   v_U16_t                  usMPDULen;
   v_U8_t                   ucMPDUHLen;
   v_U16_t                  usActualHLen = 0;   
   v_U8_t                   ucTid;
#ifdef FEATURE_WLAN_WAPI
   v_U16_t                  usEtherType = 0;
   tSirMacMgmtHdr           *hdr;
#endif
   v_U16_t                  usPktLen;
   vos_pkt_t*               vosDataBuff ;
   v_PVOID_t                aucBDHeader;
   VOS_STATUS               vosStatus;
   WLANTL_RxMetaInfoType    wRxMetaInfo;
   static v_U8_t            ucPMPDUHLen;
   v_U32_t*                 STAMetaInfoPtr;
   v_U8_t                   ucEsf=0; /* first subframe of AMSDU flag */
   v_U64_t                  ullcurrentReplayCounter=0; /*current replay counter*/
   v_U64_t                  ullpreviousReplayCounter=0; /*previous replay counter*/
   v_U16_t                  ucUnicastBroadcastType=0; /*It denotes whether received frame is UC or BC*/
   struct _BARFrmStruct     *pBarFrame = NULL;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if (( NULL == pvosDataBuff ) || ( NULL == ( vosDataBuff = *pvosDataBuff )))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid parameter sent on WLANTL_STARxAuth"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_STARxAuth"));
    return VOS_STATUS_E_FAULT;
  }
  pClientSTA = pTLCb->atlSTAClients[ucSTAId];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  /*------------------------------------------------------------------------
    Extract BD header and check if valid
   ------------------------------------------------------------------------*/
  WDA_DS_PeekRxPacketInfo( vosDataBuff, (v_PVOID_t)&aucBDHeader, 0 );

  ucMPDUHOffset = (v_U8_t)WDA_GET_RX_MPDU_HEADER_OFFSET(aucBDHeader);
  usMPDUDOffset = (v_U16_t)WDA_GET_RX_MPDU_DATA_OFFSET(aucBDHeader);
  usMPDULen     = (v_U16_t)WDA_GET_RX_MPDU_LEN(aucBDHeader);
  ucMPDUHLen    = (v_U8_t)WDA_GET_RX_MPDU_HEADER_LEN(aucBDHeader);
  ucTid         = (v_U8_t)WDA_GET_RX_TID(aucBDHeader);

  /* Fix for a hardware bug. 
   * H/W does not update the tid field in BD header for BAR frames.
   * Fix is to read the tid field from MAC header of BAR frame */
  if( (WDA_GET_RX_TYPE(aucBDHeader) == SIR_MAC_CTRL_FRAME) &&
      (WDA_GET_RX_SUBTYPE(aucBDHeader) == SIR_MAC_CTRL_BAR))
  {
      pBarFrame = (struct _BARFrmStruct *)(WDA_GET_RX_MAC_HEADER(aucBDHeader));
      ucTid = pBarFrame->barControl.numTID;
  }

  /*Host based replay check is needed for unicast data frames*/
  ucUnicastBroadcastType  = (v_U16_t)WDA_IS_RX_BCAST(aucBDHeader);
  if(0 != ucMPDUHLen)
  {
    ucPMPDUHLen = ucMPDUHLen;
  }

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:BD header processing data: HO %d DO %d Len %d HLen %d"
             " Tid %d BD %d",
             ucMPDUHOffset, usMPDUDOffset, usMPDULen, ucMPDUHLen, ucTid,
             WLANHAL_RX_BD_HEADER_SIZE));

  vos_pkt_get_packet_length( vosDataBuff, &usPktLen);

  if ( VOS_STATUS_SUCCESS != WDA_DS_TrimRxPacketInfo( vosDataBuff ) )
  {
    if((WDA_GET_RX_ASF(aucBDHeader) && !WDA_GET_RX_ESF(aucBDHeader)))
  {
    /* AMSDU case, ucMPDUHOffset = 0
     * it should be hancdled seperatly */
    if(( usMPDUDOffset >  ucMPDUHOffset ) &&
       ( usMPDULen     >= ucMPDUHLen ) && ( usPktLen >= usMPDULen ) &&
       ( !WLANTL_TID_INVALID(ucTid) ))
    {
        ucMPDUHOffset = usMPDUDOffset - WLANTL_MPDU_HEADER_LEN; 
    }
    else
    {
      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                "WLAN TL:BD header corrupted - dropping packet"));
      /* Drop packet */
      vos_pkt_return_packet(vosDataBuff);
      return VOS_STATUS_SUCCESS;
    }
  }
  else
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
              "WLAN TL:BD header corrupted - dropping packet"));
    /* Drop packet */
    vos_pkt_return_packet(vosDataBuff);
    return VOS_STATUS_SUCCESS;
  }
  }

#ifdef FEATURE_WLAN_WAPI
  if ( pClientSTA->wSTADesc.ucIsWapiSta )
  {
    vosStatus = WLANTL_GetEtherType(aucBDHeader, vosDataBuff, ucMPDUHLen, &usEtherType);
    if( VOS_IS_STATUS_SUCCESS(vosStatus) )
    {
      if ( WLANTL_LLC_WAI_TYPE  == usEtherType )
      {
        hdr = WDA_GET_RX_MAC_HEADER(aucBDHeader);
        if ( hdr->fc.wep )
        {
          TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                     "WLAN TL:WAI frame was received encrypted - dropping"));
          /* Drop packet */
          /*Temporary fix added to fix wapi rekey issue*/
          vos_pkt_return_packet(vosDataBuff);
          return vosStatus; //returning success
        }
      }
      else
      {
        if (  WLANHAL_RX_IS_UNPROTECTED_WPI_FRAME(aucBDHeader) ) 
        {
          TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                     "WLAN TL:Non-WAI frame was received unencrypted - dropping"));
          /* Drop packet */
          vos_pkt_return_packet(vosDataBuff); 
          return vosStatus; //returning success
        }
      }
    }
    else //could not extract EtherType - this should not happen
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "WLAN TL:Could not extract EtherType"));
      //Packet is already freed
      return vosStatus; //returning failure
    }
  }
#endif /* FEATURE_WLAN_WAPI */

  /*----------------------------------------------------------------------
    Increment receive counter
    !! not sure this is the best place to increase this - pkt might be
    dropped below or delayed in TL's queues
    - will leave it here for now
   ------------------------------------------------------------------------*/
  if ( !WLANTL_TID_INVALID( ucTid) ) 
  {
    pClientSTA->auRxCount[ucTid]++;
  }
  else
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Invalid tid  %d (Station ID %d) on %s",
           ucTid, ucSTAId, __func__));
  }

  /*------------------------------------------------------------------------
    Check if AMSDU and send for processing if so
   ------------------------------------------------------------------------*/
  ucAsf = (v_U8_t)WDA_GET_RX_ASF(aucBDHeader);

  if ( 0 != ucAsf )
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:Packet is AMSDU sub frame - sending for completion"));
    vosStatus = WLANTL_AMSDUProcess( pvosGCtx, &vosDataBuff, aucBDHeader, ucSTAId,
                         ucMPDUHLen, usMPDULen );
    if(NULL == vosDataBuff)
    {
       //Packet is already freed
       return VOS_STATUS_SUCCESS;
    }
  }
  /* After AMSDU header handled
   * AMSDU frame just same with normal frames */
    /*-------------------------------------------------------------------
      Translating header if necesary
       !! Fix me: rmv comments below
    ----------------------------------------------------------------------*/
  if (( 0 == WDA_GET_RX_FT_DONE(aucBDHeader) ) &&
      ( 0 != pClientSTA->wSTADesc.ucSwFrameRXXlation) &&
      ( WLANTL_IS_DATA_FRAME(WDA_GET_RX_TYPE_SUBTYPE(aucBDHeader)) ))
  {
    if(0 == ucMPDUHLen)
    {
      ucMPDUHLen = ucPMPDUHLen;
    }
    if (usMPDUDOffset > ucMPDUHOffset)
    {
      usActualHLen = usMPDUDOffset - ucMPDUHOffset;
    }
    vosStatus = WLANTL_Translate80211To8023Header( vosDataBuff, &vosStatus, usActualHLen, 
                        ucMPDUHLen, pTLCb, ucSTAId, bForwardIAPPwithLLC);

      if ( VOS_STATUS_SUCCESS != vosStatus )
      {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
               "WLAN TL:Failed to translate from 802.11 to 802.3 - dropping"));
        /* Drop packet */
        vos_pkt_return_packet(vosDataBuff);
        return vosStatus;
      }
    }
    /* Softap requires additional Info such as Destination STAID and Access
       Category. Voschain or Buffer returned by BA would be unchain and this
       Meta Data would help in routing the packets to appropriate Destination */
    if( WLAN_STA_SOFTAP == pClientSTA->wSTADesc.wSTAType)
    {
       STAMetaInfoPtr = (v_U32_t *)(uintptr_t)(ucTid | (WDA_GET_RX_ADDR3_IDX(aucBDHeader) << WLANTL_STAID_OFFSET));
       vos_pkt_set_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_TL,
                                 (v_PVOID_t)STAMetaInfoPtr);
    }

  /*------------------------------------------------------------------------
    Check to see if re-ordering session is in place
   ------------------------------------------------------------------------*/
  if ( 0 != pClientSTA->atlBAReorderInfo[ucTid].ucExists )
  {
    WLANTL_MSDUReorder( pTLCb, &vosDataBuff, aucBDHeader, ucSTAId, ucTid );
  }

if(0 == ucUnicastBroadcastType
#ifdef FEATURE_ON_CHIP_REORDERING
   && (WLANHAL_IsOnChipReorderingEnabledForTID(pvosGCtx, ucSTAId, ucTid) != TRUE)
#endif
)
{
  /* replay check code : check whether replay check is needed or not */
  if(VOS_TRUE == pClientSTA->ucIsReplayCheckValid)
  {
      /* replay check is needed for the station */

      /* check whether frame is AMSDU frame */
      if ( 0 != ucAsf )
      {
          /* Since virgo can't send AMSDU frames this leg of the code 
             was not tested properly, it needs to be tested properly*/
          /* Frame is AMSDU frame. As per 802.11n only first
             subframe will have replay counter */
          ucEsf =  WDA_GET_RX_ESF( aucBDHeader );
          if( 0 != ucEsf )
          {
              v_BOOL_t status;
              /* Getting 48-bit replay counter from the RX BD */
              ullcurrentReplayCounter = WDA_DS_GetReplayCounter(aucBDHeader);
 
              VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
               "WLAN TL: AMSDU currentReplayCounter [0x%llX]",ullcurrentReplayCounter);
              
              /* Getting 48-bit previous replay counter from TL control  block */
              ullpreviousReplayCounter = pClientSTA->ullReplayCounter[ucTid];

              VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
               "WLAN TL: AMSDU previousReplayCounter [0x%llX]",ullpreviousReplayCounter);

              /* It is first subframe of AMSDU thus it
                 conatains replay counter perform the
                 replay check for this first subframe*/
              status =  WLANTL_IsReplayPacket( ullcurrentReplayCounter, ullpreviousReplayCounter);
              if(VOS_FALSE == status)
              {
                   /* Not a replay paket, update previous replay counter in TL CB */    
                   pClientSTA->ullReplayCounter[ucTid] = ullcurrentReplayCounter;
              }
              else
              {
                  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                  "WLAN TL: AMSDU Drop the replay packet with PN : [0x%llX]",ullcurrentReplayCounter);

                  pClientSTA->ulTotalReplayPacketsDetected++;
                  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                  "WLAN TL: AMSDU total dropped replay packets on STA ID %X is [0x%X]",
                  ucSTAId,  pClientSTA->ulTotalReplayPacketsDetected);

                  /* Drop the packet */
                  vos_pkt_return_packet(vosDataBuff);
                  return VOS_STATUS_SUCCESS;
              }
          }
      }
      else
      {
           v_BOOL_t status;

           /* Getting 48-bit replay counter from the RX BD */
           ullcurrentReplayCounter = WDA_DS_GetReplayCounter(aucBDHeader);

           VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
             "WLAN TL: Non-AMSDU currentReplayCounter [0x%llX]",ullcurrentReplayCounter);

           /* Getting 48-bit previous replay counter from TL control  block */
           ullpreviousReplayCounter = pClientSTA->ullReplayCounter[ucTid];

           VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
               "WLAN TL: Non-AMSDU previousReplayCounter [0x%llX]",ullpreviousReplayCounter);

           /* It is not AMSDU frame so perform 
              reaply check for each packet, as
              each packet contains valid replay counter*/ 
           status =  WLANTL_IsReplayPacket( ullcurrentReplayCounter, ullpreviousReplayCounter);
           if(VOS_FALSE == status)
           {
                /* Not a replay paket, update previous replay counter in TL CB */    
                pClientSTA->ullReplayCounter[ucTid] = ullcurrentReplayCounter;
           }
           else
           {
              VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL: Non-AMSDU Drop the replay packet with PN : [0x%llX]",ullcurrentReplayCounter);

               pClientSTA->ulTotalReplayPacketsDetected++;
               VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL: Non-AMSDU total dropped replay packets on STA ID %X is [0x%X]",
                ucSTAId, pClientSTA->ulTotalReplayPacketsDetected);

               /* Repaly packet, drop the packet */
               vos_pkt_return_packet(vosDataBuff);
               return VOS_STATUS_SUCCESS;
           }
      }
  }
}
/*It is a broadast packet DPU has already done replay check for 
  broadcast packets no need to do replay check of these packets*/

  if ( NULL != vosDataBuff )
  {
    if( WLAN_STA_SOFTAP == pClientSTA->wSTADesc.wSTAType)
    {
      WLANTL_FwdPktToHDD( pvosGCtx, vosDataBuff, ucSTAId );
    }
    else
    {
      wRxMetaInfo.ucUP = ucTid;
      wRxMetaInfo.rssiAvg = pClientSTA->rssiAvg;
#ifdef FEATURE_WLAN_TDLS
      if (WLAN_STA_TDLS == pClientSTA->wSTADesc.wSTAType)
      {
          wRxMetaInfo.isStaTdls = TRUE;
      }
      else
      {
          wRxMetaInfo.isStaTdls = FALSE;
      }
#endif
      pClientSTA->pfnSTARx( pvosGCtx, vosDataBuff, ucSTAId,
                                            &wRxMetaInfo );
    }
  }/* if not NULL */

  return VOS_STATUS_SUCCESS;
}/* WLANTL_STARxAuth */


/*==========================================================================
  FUNCTION    WLANTL_STARxDisc

  DESCRIPTION
    Receive in disconnected state - no data allowed

  DEPENDENCIES
    The STA must be registered with TL before this function can be called.

  PARAMETERS

   IN
   pvosGCtx:       pointer to the global vos context; a handle to TL's
                   control block can be extracted from its context
   ucSTAId:        identifier of the station being processed
   vosDataBuff:   pointer to the rx vos buffer

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_STARxDisc
(
  v_PVOID_t     pvosGCtx,
  v_U8_t        ucSTAId,
  vos_pkt_t**   pvosDataBuff,
  v_BOOL_t      bForwardIAPPwithLLC
)
{
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if (( NULL == pvosDataBuff ) || ( NULL ==  *pvosDataBuff ))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid parameter sent on WLANTL_STARxDisc"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Error - drop packet
   ------------------------------------------------------------------------*/
  TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:Packet should not be received in state disconnected"
             " - dropping"));
  vos_pkt_return_packet(*pvosDataBuff);
  *pvosDataBuff = NULL;

  return VOS_STATUS_SUCCESS;
}/* WLANTL_STARxDisc */

/*==========================================================================
      Processing main loops for MAIN and TX threads
  ==========================================================================*/

/*==========================================================================
  FUNCTION    WLANTL_McProcessMsg

  DESCRIPTION
    Called by VOSS when a message was serialized for TL through the
    main thread/task.

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    message:        type and content of the message


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_McProcessMsg
(
  v_PVOID_t        pvosGCtx,
  vos_msg_t*       message
)
{
   WLANTL_CbType*  pTLCb = NULL;
   tAddBAInd*      ptAddBaInd = NULL;
   tDelBAInd*      ptDelBaInd = NULL;
   tAddBARsp*      ptAddBaRsp = NULL;
   vos_msg_t       vosMessage;
   VOS_STATUS      vosStatus;
   tpFlushACRsp FlushACRspPtr;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == message )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_ProcessMainMessage"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
     "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_ProcessMainMessage"));
    return VOS_STATUS_E_FAULT;
  }

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
        "WLAN TL:Received message: %d through main flow", message->type));

  switch( message->type )
  {
  case WDA_TL_FLUSH_AC_RSP:
    // Extract the message from the message body
    FlushACRspPtr = (tpFlushACRsp)(message->bodyptr);
    // Make sure the call back function is not null.
    if ( NULL == pTLCb->tlBAPClient.pfnFlushOpCompleteCb )
    {
      VOS_ASSERT(0);
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
       "WLAN TL:Invalid TL pointer pfnFlushOpCompleteCb"));
      return VOS_STATUS_E_FAULT;
    }

    TLLOG2(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
            "Received message:  Flush complete received by TL"));

    // Since we have the response back from HAL, just call the BAP client
    // registered call back from TL. There is only 1 possible
    // BAP client. So directly reference tlBAPClient
    pTLCb->tlBAPClient.pfnFlushOpCompleteCb( pvosGCtx,
            FlushACRspPtr->ucSTAId,
            FlushACRspPtr->ucTid, FlushACRspPtr->status );

    // Free the PAL memory, we are done with it.
    TLLOG2(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
        "Flush complete received by TL: Freeing %p", FlushACRspPtr));
    vos_mem_free((v_VOID_t *)FlushACRspPtr);
    break;

  case WDA_HDD_ADDBA_REQ:
   ptAddBaInd = (tAddBAInd*)(message->bodyptr);
    vosStatus = WLANTL_BaSessionAdd( pvosGCtx,
                                 ptAddBaInd->baSession.baSessionID,
                                     ptAddBaInd->baSession.STAID,
                                     ptAddBaInd->baSession.baTID,
                                 (v_U32_t)ptAddBaInd->baSession.baBufferSize,
                                 ptAddBaInd->baSession.winSize,
                                 ptAddBaInd->baSession.SSN);
    ptAddBaRsp = vos_mem_malloc(sizeof(*ptAddBaRsp));

    if ( NULL == ptAddBaRsp )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL: fatal failure, cannot allocate BA Rsp structure"));
      VOS_ASSERT(0);
      return VOS_STATUS_E_NOMEM;
    }

    if ( VOS_STATUS_SUCCESS == vosStatus )
    {
      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                "WLAN TL: Sending success indication to HAL for ADD BA"));
      /*Send success*/
      ptAddBaRsp->mesgType    = WDA_HDD_ADDBA_RSP;
      vosMessage.type         = WDA_HDD_ADDBA_RSP;
    }
    else
    {
      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLAN TL: Sending failure indication to HAL for ADD BA"));

      /*Send failure*/
      ptAddBaRsp->mesgType    = WDA_BA_FAIL_IND;
      vosMessage.type         = WDA_BA_FAIL_IND;
    }

    ptAddBaRsp->mesgLen     = sizeof(tAddBARsp);
    ptAddBaRsp->baSessionID = ptAddBaInd->baSession.baSessionID;
      /* This is default, reply win size has to be handled BA module, FIX THIS */
      ptAddBaRsp->replyWinSize = WLANTL_MAX_WINSIZE;
    vosMessage.bodyptr = ptAddBaRsp;

    vos_mq_post_message( VOS_MQ_ID_WDA, &vosMessage );
    WLANTL_McFreeMsg (pvosGCtx, message);
  break;
  case WDA_DELETEBA_IND:
    ptDelBaInd = (tDelBAInd*)(message->bodyptr);
    vosStatus  = WLANTL_BaSessionDel(pvosGCtx,
                                 ptDelBaInd->staIdx,
                                 ptDelBaInd->baTID);

    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL: Failed to del BA session STA:%d TID:%d Status :%d",
               ptDelBaInd->staIdx,
               ptDelBaInd->baTID,
               vosStatus));
    }
    WLANTL_McFreeMsg (pvosGCtx, message);
    break;
  default:
    /*no processing for now*/
    break;
  }

  return VOS_STATUS_SUCCESS;
}/* WLANTL_ProcessMainMessage */

/*==========================================================================
  FUNCTION    WLANTL_McFreeMsg

  DESCRIPTION
    Called by VOSS to free a given TL message on the Main thread when there
    are messages pending in the queue when the whole system is been reset.
    For now, TL does not allocate any body so this function shout translate
    into a NOOP

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    message:        type and content of the message


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_McFreeMsg
(
  v_PVOID_t        pvosGCtx,
  vos_msg_t*       message
)
{
  WLANTL_CbType*  pTLCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

   /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == message )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_McFreeMsg"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
     "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_McFreeMsg"));
    return VOS_STATUS_E_FAULT;
  }

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
        "WLAN TL:Received message: %d through main free", message->type));

  switch( message->type )
  {
  case WDA_HDD_ADDBA_REQ:
  case WDA_DELETEBA_IND:
    /*vos free body pointer*/
    vos_mem_free(message->bodyptr);
    message->bodyptr = NULL;
    break;
  default:
    /*no processing for now*/
    break;
  }

  return VOS_STATUS_SUCCESS;
}/*WLANTL_McFreeMsg*/

/*==========================================================================
  FUNCTION    WLANTL_TxProcessMsg

  DESCRIPTION
    Called by VOSS when a message was serialized for TL through the
    tx thread/task.

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    message:        type and content of the message


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  Other values can be returned as a result of a function call, please check
  corresponding API for more info.
  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_TxProcessMsg
(
  v_PVOID_t        pvosGCtx,
  vos_msg_t*       message
)
{
   VOS_STATUS      vosStatus = VOS_STATUS_SUCCESS;
   void (*callbackRoutine) (void *callbackContext);
   void *callbackContext;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == message )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_ProcessTxMessage"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Process message
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Received message: %d through tx flow", message->type));

  switch( message->type )
  {
  case WLANTL_TX_SIG_SUSPEND:
    vosStatus = WLANTL_SuspendCB( pvosGCtx,
                                 (WLANTL_SuspendCBType)message->bodyptr,
                                 message->reserved);
    break;
  case WLANTL_TX_RES_NEEDED:
    vosStatus = WLANTL_GetTxResourcesCB( pvosGCtx );
     break;

  case WDA_DS_TX_START_XMIT:
      WLANTL_ClearTxXmitPending(pvosGCtx);
      vosStatus = WDA_DS_TxFrames( pvosGCtx );
      break;

  case WDA_DS_FINISH_ULA:
    callbackContext = message->bodyptr;
    callbackRoutine = message->callback;
    if ( NULL != callbackRoutine )
    {
      callbackRoutine(callbackContext);
    }
    break;

  case WLANTL_TX_SNAPSHOT:
    /*Dumping TL State and then continuing to print
      the DXE Dump*/
    WLANTL_TxThreadDebugHandler(pvosGCtx);
    WDA_TransportChannelDebug(NULL, VOS_TRUE, VOS_FALSE);
    break;

  case WLANTL_TX_FATAL_ERROR:
    WLANTL_FatalErrorHandler(pvosGCtx);
    break;

  case WLANTL_TX_FW_DEBUG:
    vos_fwDumpReq(274, 0, 0, 0, 0, 1); //Async event
    break;

  case WLANTL_TX_KICKDXE:
    WDA_TransportKickDxe();
    break;

  default:
    /*no processing for now*/
    break;
  }

  return vosStatus;
}/* WLANTL_TxProcessMsg */

/*==========================================================================
  FUNCTION    WLANTL_McFreeMsg

  DESCRIPTION
    Called by VOSS to free a given TL message on the Main thread when there
    are messages pending in the queue when the whole system is been reset.
    For now, TL does not allocate any body so this function shout translate
    into a NOOP

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    message:        type and content of the message


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_TxFreeMsg
(
  v_PVOID_t        pvosGCtx,
  vos_msg_t*       message
)
{
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*Nothing to do for now!!!*/
  return VOS_STATUS_SUCCESS;
}/*WLANTL_TxFreeMsg*/

/*==========================================================================

  FUNCTION    WLANTL_TxFCFrame

  DESCRIPTION
    Internal utility function to send FC frame. Enable
    or disable LWM mode based on the information.

  DEPENDENCIES
    TL must be initiailized before this function gets called.
    FW sends up special flow control frame.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   Input pointers are NULL.
    VOS_STATUS_E_FAULT:   Something is wrong.
    VOS_STATUS_SUCCESS:   Everything is good.

  SIDE EFFECTS
    Newly formed FC frame is generated and waits to be transmitted. Previously unsent frame will
    be released.

============================================================================*/
VOS_STATUS
WLANTL_TxFCFrame
(
  v_PVOID_t       pvosGCtx
)
{
#if 0
  WLANTL_CbType*      pTLCb = NULL;
  VOS_STATUS          vosStatus;
  tpHalFcTxBd         pvFcTxBd = NULL;
  vos_pkt_t *         pPacket = NULL;
  v_U8_t              ucSTAId = 0;
  v_U8_t              ucBitCheck = 1;

  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,
               "WLAN TL: Send FC frame %s", __func__);

  /*------------------------------------------------------------------------
    Sanity check
    ------------------------------------------------------------------------*/
  if ( NULL == pvosGCtx )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter %s", __func__));
    return VOS_STATUS_E_INVAL;
  }
  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);

  if (NULL == pTLCb)
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL:Invalid pointer in %s \n", __func__));
    return VOS_STATUS_E_INVAL;
  }
  
  //Get one voss packet
  vosStatus = vos_pkt_get_packet( &pPacket, VOS_PKT_TYPE_TX_802_11_MGMT, sizeof(tHalFcTxBd), 1, 
                                    VOS_FALSE, NULL, NULL );

  if ( VOS_STATUS_SUCCESS != vosStatus )
  {
    return VOS_STATUS_E_INVAL;
  }

  vosStatus = vos_pkt_reserve_head( pPacket, (void *)&pvFcTxBd, sizeof(tHalFcTxBd));

  if( VOS_STATUS_SUCCESS != vosStatus )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "%s: failed to reserve FC TX BD %d\n",__func__, sizeof(tHalFcTxBd)));
    vos_pkt_return_packet( pPacket );
    return VOS_STATUS_E_FAULT;
  }

  //Generate most recent tlFCInfo. Most fields are correct.
  pTLCb->tlFCInfo.fcSTAThreshEnabledMask = 0;
  pTLCb->tlFCInfo.fcSTATxMoreDataMask = 0;
  for( ucSTAId = 0, ucBitCheck = 1 ; ucSTAId < WLAN_MAX_STA_COUNT; ucBitCheck <<= 1, ucSTAId ++)
  {
    if (0 == pTLCb->atlSTAClients[ucSTAId].ucExists)
    {
      continue;
    }

    if (pTLCb->atlSTAClients[ucSTAId].ucPktPending)
    {
      pTLCb->tlFCInfo.fcSTATxMoreDataMask |= ucBitCheck;
    }

    if ( (pTLCb->atlSTAClients[ucSTAId].ucLwmModeEnabled) &&
         (pTLCb->atlSTAClients[ucSTAId].bmuMemConsumed > pTLCb->atlSTAClients[ucSTAId].uLwmThreshold))
    {
      pTLCb->tlFCInfo.fcSTAThreshEnabledMask |= ucBitCheck;

      pTLCb->tlFCInfo.fcSTAThresh[ucSTAId] = (tANI_U8)pTLCb->atlSTAClients[ucSTAId].uLwmThreshold;

      pTLCb->atlSTAClients[ucSTAId].ucLwmEventReported = FALSE;
    }

  }
  
  //request immediate feedback
  pTLCb->tlFCInfo.fcConfig |= 0x4;                               

  //fill in BD to sent
  vosStatus = WLANHAL_FillFcTxBd(pvosGCtx, &pTLCb->tlFCInfo, (void *)pvFcTxBd);

  if( VOS_STATUS_SUCCESS != vosStatus )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "%s: Fill FC TX BD unsuccessful\n", __func__));
    vos_pkt_return_packet( pPacket );
    return VOS_STATUS_E_FAULT;
  }

  if (NULL != pTLCb->vosTxFCBuf)
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "%s: Previous FC TX BD not sent\n", __func__));
    vos_pkt_return_packet(pTLCb->vosTxFCBuf);
  }

  pTLCb->vosTxFCBuf = pPacket;

  vos_pkt_set_user_data_ptr( pPacket, VOS_PKT_USER_DATA_ID_TL,
                               (v_PVOID_t)WLANTL_TxCompDefaultCb);
  vosStatus = WDA_DS_StartXmit(pvosGCtx);

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL: send FC frame leave %s", __func__));
#endif
  return VOS_STATUS_SUCCESS;
}


/*==========================================================================
  FUNCTION    WLANTL_GetTxResourcesCB

  DESCRIPTION
    Processing function for Resource needed signal. A request will be issued
    to BAL to get more tx resources.

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:   pointer to TL cb is NULL ; access would cause a
                          page fault
    VOS_STATUS_SUCCESS:   Everything is good :)

  Other values can be returned as a result of a function call, please check
  corresponding API for more info.
  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetTxResourcesCB
(
  v_PVOID_t        pvosGCtx
)
{
  WLANTL_CbType*  pTLCb      = NULL;
  v_U32_t         uResCount  = WDA_TLI_MIN_RES_DATA;
  VOS_STATUS      vosStatus  = VOS_STATUS_SUCCESS;
  v_U8_t          ucMgmt     = 0;
  v_U8_t          ucBAP      = 0;
  v_U8_t          ucData     = 0;
#ifdef WLAN_SOFTAP_FLOWCTRL_EN
  tBssSystemRole systemRole;
  tpAniSirGlobal pMac;
#endif
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
  /*------------------------------------------------------------------------
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid TL pointer from pvosGCtx on"
               " WLANTL_ProcessTxMessage"));
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Get tx resources from BAL
   ------------------------------------------------------------------------*/
  vosStatus = WDA_DS_GetTxResources( pvosGCtx, &uResCount );

  if ( (VOS_STATUS_SUCCESS != vosStatus) && (VOS_STATUS_E_RESOURCES != vosStatus))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:TL failed to get resources from BAL, Err: %d",
               vosStatus));
    return vosStatus;
  }

  /* Currently only Linux BAL returns the E_RESOURCES error code when it is running 
     out of BD/PDUs. To make use of this interrupt for throughput enhancement, similar
     changes should be done in BAL code of AMSS and WM */
  if (VOS_STATUS_E_RESOURCES == vosStatus)
  {
#ifdef VOLANS_PERF
     WLANHAL_EnableIdleBdPduInterrupt(pvosGCtx, (tANI_U8)bdPduInterruptGetThreshold);
     VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL: Enabling Idle BD/PDU interrupt, Current resources = %d", uResCount);
#else
    return VOS_STATUS_E_FAILURE;
#endif
  }

  pTLCb->uResCount = uResCount;
  

#ifdef WLAN_SOFTAP_FLOWCTRL_EN
  /* FIXME: disabled since creating issues in power-save, needs to be addressed */ 
  pTLCb->sendFCFrame ++;
  pMac = vos_get_context(VOS_MODULE_ID_WDA, pvosGCtx);
  systemRole = wdaGetGlobalSystemRole(pMac);
  if (eSYSTEM_AP_ROLE == systemRole)
  {
     if (pTLCb->sendFCFrame % 16 == 0)
     {
         TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                "Transmit FC"));
         WLANTL_TxFCFrame (pvosGCtx);
     } 
  }
#endif //WLAN_SOFTAP_FLOWCTRL_EN 

  ucData = ( pTLCb->uResCount >=  WDA_TLI_MIN_RES_DATA );
  ucBAP  = ( pTLCb->uResCount >=  WDA_TLI_MIN_RES_BAP ) &&
           ( NULL != pTLCb->tlBAPClient.vosPendingDataBuff );
  ucMgmt = ( pTLCb->uResCount >=  WDA_TLI_MIN_RES_MF ) &&
           ( NULL != pTLCb->tlMgmtFrmClient.vosPendingDataBuff );

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL: Eval Resume tx Res: %d DATA: %d BAP: %d MGMT: %d",
             pTLCb->uResCount, ucData, ucBAP, ucMgmt));

  if (( 0 == pTLCb->ucTxSuspended ) &&
      (( 0 != ucData ) || ( 0 != ucMgmt ) || ( 0 != ucBAP ) ) )
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "Issuing Xmit start request to BAL for avail res SYNC"));
    vosStatus =WDA_DS_StartXmit(pvosGCtx);
  }
  return vosStatus;
}/*WLANTL_GetTxResourcesCB*/

/*==========================================================================
      Utility functions
  ==========================================================================*/

/*==========================================================================
  FUNCTION    WLANTL_Translate8023To80211Header

  DESCRIPTION
    Inline function for translating and 802.11 header into an 802.3 header.

  DEPENDENCIES


  PARAMETERS

   IN
    pTLCb:            TL control block
   IN/OUT 
    ucStaId:          station ID. Incase of TDLS, this returns actual TDLS
                      station ID used

   IN/OUT
    vosDataBuff:      vos data buffer, will contain the new header on output

   OUT
    pvosStatus:       status of the operation

  RETURN VALUE

    VOS_STATUS_SUCCESS:  Everything is good :)

    Other error codes might be returned from the vos api used in the function
    please check those return values.

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_Translate8023To80211Header
(
  vos_pkt_t*      vosDataBuff,
  VOS_STATUS*     pvosStatus,
  WLANTL_CbType*  pTLCb,
  v_U8_t          *pucStaId,
  WLANTL_MetaInfoType *tlMetaInfo,
  v_U8_t          *ucWDSEnabled,
  v_U8_t          *extraHeadSpace
)
{
  WLANTL_8023HeaderType  w8023Header;
  WLANTL_80211HeaderType *pw80211Header; // Allocate an aligned BD and then fill it. 
  VOS_STATUS             vosStatus;
  v_U8_t                 MandatoryucHeaderSize = WLAN80211_MANDATORY_HEADER_SIZE;
  v_U8_t                 ucHeaderSize = 0;
  v_VOID_t               *ppvBDHeader = NULL;
  WLANTL_STAClientType*  pClientSTA = NULL;
  v_U8_t                 ucQoSOffset = WLAN80211_MANDATORY_HEADER_SIZE;
  v_U8_t                 ucStaId;
#ifdef FEATURE_WLAN_ESE_UPLOAD
  v_BOOL_t               bIAPPTxwithLLC = VOS_FALSE;
  v_SIZE_t               wIAPPSnapSize = WLANTL_LLC_HEADER_LEN;
  v_U8_t                 wIAPPSnap[WLANTL_LLC_HEADER_LEN] = {0};
#endif
  *ucWDSEnabled = 0; // default WDS off.
  vosStatus = vos_pkt_pop_head( vosDataBuff, &w8023Header,
                                sizeof(w8023Header));

  if ( VOS_STATUS_SUCCESS != vosStatus )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
     "WLAN TL: Packet pop header fails on WLANTL_Translate8023To80211Header"));
     return vosStatus;
  }

  if( NULL == pucStaId )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
     "WLAN TL: Invalid pointer for StaId"));
     return VOS_STATUS_E_INVAL;
  }
  ucStaId = *pucStaId;
  pClientSTA = pTLCb->atlSTAClients[ucStaId];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

#ifdef FEATURE_WLAN_TDLS

  if ( WLAN_STA_INFRA == pTLCb->atlSTAClients[ucStaId]->wSTADesc.wSTAType
      && pTLCb->ucTdlsPeerCount )
  {
    v_U8_t ucIndex = 0;
    for ( ucIndex = 0; ucIndex < WLAN_MAX_STA_COUNT ; ucIndex++)
    {
      if ( ucIndex != ucStaId && pTLCb->atlSTAClients[ucIndex] && pTLCb->atlSTAClients[ucIndex]->ucExists &&
          (pTLCb->atlSTAClients[ucIndex]->tlState == WLANTL_STA_AUTHENTICATED) &&
          (!pTLCb->atlSTAClients[ucIndex]->ucTxSuspended) &&
          vos_mem_compare( (void*)pTLCb->atlSTAClients[ucIndex]->wSTADesc.vSTAMACAddress.bytes,
            (void*)w8023Header.vDA, 6) )
      {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
              "WLAN TL: Got a TDLS station. Using that index"));
        ucStaId = ucIndex;
        *pucStaId = ucStaId;
        pClientSTA = pTLCb->atlSTAClients[ucStaId];
        if ( NULL == pClientSTA )
        {
            TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:Client Memory was not allocated on %s", __func__));
            return VOS_STATUS_E_FAILURE;
        }
        break;
      }
    }
  }
#endif

#ifdef FEATURE_WLAN_ESE_UPLOAD
if ((0 == w8023Header.usLenType) && (pClientSTA->wSTADesc.ucIsEseSta))
{
    vos_pkt_extract_data(vosDataBuff,0,&wIAPPSnap[0],&wIAPPSnapSize);
    if (vos_mem_compare(wIAPPSnap,WLANTL_AIRONET_SNAP_HEADER,WLANTL_LLC_HEADER_LEN))
    {
        /*The SNAP and the protocol type are already in the data buffer.
         They are filled by the application (wpa_supplicant). So, Skip Adding LLC below.*/
         bIAPPTxwithLLC = VOS_TRUE;
    }
    else
    {
        bIAPPTxwithLLC = VOS_FALSE;
    }
}
#endif /* FEATURE_WLAN_ESE_UPLOAD */

  if ((0 != pClientSTA->wSTADesc.ucAddRmvLLC)
#ifdef FEATURE_WLAN_ESE_UPLOAD
      && (!bIAPPTxwithLLC)
#endif /* FEATURE_WLAN_ESE_UPLOAD */
     )
  {
    /* Push the length */
    vosStatus = vos_pkt_push_head(vosDataBuff,
                    &w8023Header.usLenType, sizeof(w8023Header.usLenType));

    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                  "WLAN TL: Packet push ether type fails on"
                  " WLANTL_Translate8023To80211Header"));
       return vosStatus;
    }

#ifdef BTAMP_TEST
    // The STA side will execute this, a hack to test BTAMP by using the
    // infra setup. On real BTAMP this will come from BAP itself.
    {
    static v_U8_t WLANTL_BT_AMP_LLC_HEADER[] =  {0xAA, 0xAA, 0x03, 0x00, 0x19, 0x58 };
    vosStatus = vos_pkt_push_head(vosDataBuff, WLANTL_BT_AMP_LLC_HEADER,
                       sizeof(WLANTL_BT_AMP_LLC_HEADER));

    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "WLAN TL: Packet push LLC header fails on"
                  " WLANTL_Translate8023To80211Header"));
       return vosStatus;
    }
    }
#else
    vosStatus = vos_pkt_push_head(vosDataBuff, WLANTL_LLC_HEADER,
                       sizeof(WLANTL_LLC_HEADER));

    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "WLAN TL: Packet push LLC header fails on"
                  " WLANTL_Translate8023To80211Header"));
       return vosStatus;
    }
#endif
  }/*If add LLC is enabled*/
  else
  {
#ifdef FEATURE_WLAN_ESE_UPLOAD
      bIAPPTxwithLLC = VOS_FALSE; /*Reset the Flag here to start afresh with the next TX pkt*/
#endif /* FEATURE_WLAN_ESE_UPLOAD */
       TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLAN TL: STA Client registered to not remove LLC"
                  " WLANTL_Translate8023To80211Header"));
  }

#ifdef BTAMP_TEST
  pClientSTA->wSTADesc.wSTAType = WLAN_STA_BT_AMP;
#endif

  // Find the space required for the 802.11 header format
  // based on the frame control fields.
  ucHeaderSize = MandatoryucHeaderSize;
  if (pClientSTA->wSTADesc.ucQosEnabled)
  {  
    ucHeaderSize += sizeof(pw80211Header->usQosCtrl);
  }
  if (pClientSTA->wSTADesc.wSTAType == WLAN_STA_BT_AMP)
  {  
    ucHeaderSize += sizeof(pw80211Header->optvA4);
    ucQoSOffset += sizeof(pw80211Header->optvA4);
  }

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
      " WLANTL_Translate8023To80211Header : Header size = %d ", ucHeaderSize));

  vos_pkt_reserve_head( vosDataBuff, &ppvBDHeader, ucHeaderSize );
  if ( NULL == ppvBDHeader )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:VOSS packet corrupted "));
    *pvosStatus = VOS_STATUS_E_INVAL;
    return *pvosStatus;
  }


  // OK now we have the space. Fill the 80211 header
  /* Fill A2 */
  pw80211Header = (WLANTL_80211HeaderType *)(ppvBDHeader);
  // only clear the required space.
  vos_mem_set( pw80211Header, ucHeaderSize, 0 );
  vos_mem_copy( pw80211Header->vA2, w8023Header.vSA,  VOS_MAC_ADDR_SIZE);


#ifdef FEATURE_WLAN_WAPI
  if (( WLANTL_STA_AUTHENTICATED == pClientSTA->tlState ||
        pClientSTA->ptkInstalled ) && (tlMetaInfo->ucIsWai != 1))
#else
  if ( WLANTL_STA_AUTHENTICATED == pClientSTA->tlState ||
       pClientSTA->ptkInstalled )
#endif
  {
    pw80211Header->wFrmCtrl.wep =
                 pClientSTA->wSTADesc.ucProtectedFrame;
  }

  pw80211Header->usDurationId = 0;
  pw80211Header->usSeqCtrl    = 0;

  pw80211Header->wFrmCtrl.type     = WLANTL_80211_DATA_TYPE;



  if(pClientSTA->wSTADesc.ucQosEnabled)
  {
      pw80211Header->wFrmCtrl.subType  = WLANTL_80211_DATA_QOS_SUBTYPE;

      *((v_U16_t *)((v_U8_t *)ppvBDHeader + ucQoSOffset)) = tlMetaInfo->ucUP;

  }
  else
  {
      pw80211Header->wFrmCtrl.subType  = 0;
      tlMetaInfo->ucUP = 0;
      tlMetaInfo->ucTID = 0;

  // NO NO NO - there is not enough memory allocated to write the QOS ctrl  
  // field, it will overwrite the first 2 bytes of the data packet(LLC header)
  // pw80211Header->usQosCtrl         = 0;
  }


  switch( pClientSTA->wSTADesc.wSTAType )
  {
     case WLAN_STA_IBSS:
        pw80211Header->wFrmCtrl.toDS          = 0;
        pw80211Header->wFrmCtrl.fromDS        = 0;

        vos_copy_macaddr( (v_MACADDR_t*)&pw80211Header->vA1,
                             (v_MACADDR_t*)&w8023Header.vDA);
        vos_mem_copy( pw80211Header->vA3,
              &pClientSTA->wSTADesc.vBSSIDforIBSS ,
              VOS_MAC_ADDR_SIZE);
        break;

     case WLAN_STA_BT_AMP:
        *ucWDSEnabled = 1; // WDS on.
        pw80211Header->wFrmCtrl.toDS          = 1;
        pw80211Header->wFrmCtrl.fromDS        = 1;
        vos_copy_macaddr( (v_MACADDR_t*)&pw80211Header->vA1,
              &pClientSTA->wSTADesc.vSTAMACAddress);
        vos_mem_copy( pw80211Header->vA2,
              w8023Header.vSA, VOS_MAC_ADDR_SIZE);
        vos_copy_macaddr( (v_MACADDR_t*)&pw80211Header->vA3,
              &pClientSTA->wSTADesc.vSTAMACAddress);
        /* fill the optional A4 header */
        vos_mem_copy( pw80211Header->optvA4,
              w8023Header.vSA, VOS_MAC_ADDR_SIZE);
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "BTAMP CASE NOW ---------staid=%d",
                 ucStaId));
        break;

     case WLAN_STA_SOFTAP:
        *ucWDSEnabled = 0; // WDS off.
        pw80211Header->wFrmCtrl.toDS          = 0;
        pw80211Header->wFrmCtrl.fromDS        = 1;
        /*Copy the DA to A1*/
        vos_mem_copy( pw80211Header->vA1, w8023Header.vDA , VOS_MAC_ADDR_SIZE);   
        vos_copy_macaddr( (v_MACADDR_t*)&pw80211Header->vA2,
              &pClientSTA->wSTADesc.vSelfMACAddress);
        vos_mem_copy( pw80211Header->vA3,
              w8023Header.vSA, VOS_MAC_ADDR_SIZE);
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "sw 802 to 80211 softap case  ---------staid=%d",
                 ucStaId));
        break;
#ifdef FEATURE_WLAN_TDLS
     case WLAN_STA_TDLS:
        pw80211Header->wFrmCtrl.toDS          = 0;
        pw80211Header->wFrmCtrl.fromDS        = 0;
        /*Fix me*/
        vos_copy_macaddr( (v_MACADDR_t*)&pw80211Header->vA1,
              &pClientSTA->wSTADesc.vSTAMACAddress);
        vos_mem_copy( pw80211Header->vA3,
              &pClientSTA->wSTADesc.vBSSIDforIBSS ,
              VOS_MAC_ADDR_SIZE);
        VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
              ("TL:TDLS CASE NOW ---------staid=%d"), ucStaId);
        break;
#endif
     case WLAN_STA_INFRA:
     default:
        pw80211Header->wFrmCtrl.toDS          = 1;
        pw80211Header->wFrmCtrl.fromDS        = 0;
        vos_copy_macaddr( (v_MACADDR_t*)&pw80211Header->vA1,
              &pClientSTA->wSTADesc.vSTAMACAddress);
        vos_mem_copy( pw80211Header->vA3, w8023Header.vDA , VOS_MAC_ADDR_SIZE);
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "REGULAR INFRA LINK CASE---------staid=%d",
                 ucStaId));
        break;
  }
  // OK now we have the space. Fill the 80211 header
  /* Fill A2 */
  pw80211Header = (WLANTL_80211HeaderType *)(ppvBDHeader);
  return VOS_STATUS_SUCCESS;
}/*WLANTL_Translate8023To80211Header*/


/*=============================================================================
   BEGIN LOG FUNCTION    !!! Remove me or clean me
=============================================================================*/
#if 0 //def WLANTL_DEBUG

#define WLANTL_DEBUG_FRAME_BYTE_PER_LINE    16
#define WLANTL_DEBUG_FRAME_BYTE_PER_BYTE    4

static v_VOID_t WLANTL_DebugFrame
(
   v_PVOID_t   dataPointer,
   v_U32_t     dataSize
)
{
   v_U8_t   lineBuffer[WLANTL_DEBUG_FRAME_BYTE_PER_LINE];
   v_U32_t  numLines;
   v_U32_t  numBytes;
   v_U32_t  idx;
   v_U8_t   *linePointer;

   numLines = dataSize / WLANTL_DEBUG_FRAME_BYTE_PER_LINE;
   numBytes = dataSize % WLANTL_DEBUG_FRAME_BYTE_PER_LINE;
   linePointer = (v_U8_t *)dataPointer;

   TLLOGE(VOS_TRACE(VOS_MODULE_ID_SAL, VOS_TRACE_LEVEL_ERROR, "WLAN TL:Frame Debug Frame Size %d, Pointer 0x%p", dataSize, dataPointer));
   for(idx = 0; idx < numLines; idx++)
   {
      memset(lineBuffer, 0, WLANTL_DEBUG_FRAME_BYTE_PER_LINE);
      memcpy(lineBuffer, linePointer, WLANTL_DEBUG_FRAME_BYTE_PER_LINE);
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_SAL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x",
                 lineBuffer[0], lineBuffer[1], lineBuffer[2], lineBuffer[3], lineBuffer[4], lineBuffer[5], lineBuffer[6], lineBuffer[7],
                 lineBuffer[8], lineBuffer[9], lineBuffer[10], lineBuffer[11], lineBuffer[12], lineBuffer[13], lineBuffer[14], lineBuffer[15]));
      linePointer += WLANTL_DEBUG_FRAME_BYTE_PER_LINE;
   }

   if(0 == numBytes)
      return;

   memset(lineBuffer, 0, WLANTL_DEBUG_FRAME_BYTE_PER_LINE);
   memcpy(lineBuffer, linePointer, numBytes);
   for(idx = 0; idx < WLANTL_DEBUG_FRAME_BYTE_PER_LINE / WLANTL_DEBUG_FRAME_BYTE_PER_BYTE; idx++)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_SAL, VOS_TRACE_LEVEL_ERROR, "WLAN TL:0x%2x 0x%2x 0x%2x 0x%2x",
                lineBuffer[idx * WLANTL_DEBUG_FRAME_BYTE_PER_BYTE], lineBuffer[1 + idx * WLANTL_DEBUG_FRAME_BYTE_PER_BYTE],
                lineBuffer[2 + idx * WLANTL_DEBUG_FRAME_BYTE_PER_BYTE], lineBuffer[3 + idx * WLANTL_DEBUG_FRAME_BYTE_PER_BYTE]));
      if(((idx + 1) * WLANTL_DEBUG_FRAME_BYTE_PER_BYTE) >= numBytes)
         break;
   }

   return;
}
#endif

/*=============================================================================
   END LOG FUNCTION
=============================================================================*/

/*==========================================================================
  FUNCTION    WLANTL_Translate80211To8023Header

  DESCRIPTION
    Inline function for translating and 802.11 header into an 802.3 header.

  DEPENDENCIES


  PARAMETERS

   IN
    pTLCb:            TL control block
    ucStaId:          station ID
    ucHeaderLen:      Length of the header from BD
    ucActualHLen:     Length of header including padding or any other trailers

   IN/OUT
    vosDataBuff:      vos data buffer, will contain the new header on output

   OUT
    pvosStatus:       status of the operation

   RETURN VALUE

    The result code associated with performing the operation
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_Translate80211To8023Header
(
  vos_pkt_t*      vosDataBuff,
  VOS_STATUS*     pvosStatus,
  v_U16_t         usActualHLen,
  v_U8_t          ucHeaderLen,
  WLANTL_CbType*  pTLCb,
  v_U8_t          ucSTAId,
  v_BOOL_t        bForwardIAPPwithLLC
)
{
  WLANTL_8023HeaderType  w8023Header;
  WLANTL_80211HeaderType w80211Header;
  v_U8_t                 aucLLCHeader[WLANTL_LLC_HEADER_LEN];
  VOS_STATUS             vosStatus;
  v_U16_t                usDataStartOffset = 0;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  if ( sizeof(w80211Header) < ucHeaderLen )
  {
     TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
       "Warning !: Check the header size for the Rx frame structure=%d received=%dn",
       sizeof(w80211Header), ucHeaderLen));
     ucHeaderLen = sizeof(w80211Header);
  }

  // This will take care of headers of all sizes, 3 address, 3 addr QOS,
  // WDS non-QOS and WDS QoS etc. We have space for all in the 802.11 header structure.
  vosStatus = vos_pkt_pop_head( vosDataBuff, &w80211Header, ucHeaderLen);

  if ( VOS_STATUS_SUCCESS != vosStatus )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL: Failed to pop 80211 header from packet %d",
                vosStatus));

     return vosStatus;
  }

  switch ( w80211Header.wFrmCtrl.fromDS )
  {
  case 0:
    if ( w80211Header.wFrmCtrl.toDS )
    {
      //SoftAP AP mode
      vos_mem_copy( w8023Header.vDA, w80211Header.vA3, VOS_MAC_ADDR_SIZE);
      vos_mem_copy( w8023Header.vSA, w80211Header.vA2, VOS_MAC_ADDR_SIZE);
      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                  "WLAN TL SoftAP: 802 3 DA %08x SA %08x",
                  w8023Header.vDA, w8023Header.vSA));
    }
    else 
    {
      /* IBSS */
      vos_mem_copy( w8023Header.vDA, w80211Header.vA1, VOS_MAC_ADDR_SIZE);
      vos_mem_copy( w8023Header.vSA, w80211Header.vA2, VOS_MAC_ADDR_SIZE);
    }
    break;
  case 1:
    if ( w80211Header.wFrmCtrl.toDS )
    {
      /* BT-AMP case */
      vos_mem_copy( w8023Header.vDA, w80211Header.vA1, VOS_MAC_ADDR_SIZE);
      vos_mem_copy( w8023Header.vSA, w80211Header.vA2, VOS_MAC_ADDR_SIZE);
    }
    else
    { /* Infra */
      vos_mem_copy( w8023Header.vDA, w80211Header.vA1, VOS_MAC_ADDR_SIZE);
      vos_mem_copy( w8023Header.vSA, w80211Header.vA3, VOS_MAC_ADDR_SIZE);
    }
    break;
  }

  if( usActualHLen > ucHeaderLen )
  {
     usDataStartOffset = usActualHLen - ucHeaderLen;
  }

  if ( 0 < usDataStartOffset )
  {
    vosStatus = vos_pkt_trim_head( vosDataBuff, usDataStartOffset );

    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                  "WLAN TL: Failed to trim header from packet %d",
                  vosStatus));
      return vosStatus;
    }
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  if ( 0 != pTLCb->atlSTAClients[ucSTAId]->wSTADesc.ucAddRmvLLC
#ifdef FEATURE_WLAN_ESE_UPLOAD
    && (!bForwardIAPPwithLLC)
#endif /*  FEATURE_WLAN_ESE_UPLOAD */
     )
  {
    // Extract the LLC header
    vosStatus = vos_pkt_pop_head( vosDataBuff, aucLLCHeader,
                                  WLANTL_LLC_HEADER_LEN);

    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                  "WLAN TL: Failed to pop LLC header from packet %d",
                  vosStatus));

       return vosStatus;
    }

    //Extract the length
    vos_mem_copy(&w8023Header.usLenType,
      &aucLLCHeader[WLANTL_LLC_HEADER_LEN - sizeof(w8023Header.usLenType)],
      sizeof(w8023Header.usLenType) );
  }
  else
  {
    vosStatus = vos_pkt_get_packet_length(vosDataBuff,
                                        &w8023Header.usLenType);

    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                  "WLAN TL: Failed to get packet length %d",
                  vosStatus));

       return vosStatus;
    }

    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
        "WLAN TL: BTAMP len (ethertype) fld = %d",
        w8023Header.usLenType));
    w8023Header.usLenType = vos_cpu_to_be16(w8023Header.usLenType);
  }

  vos_pkt_push_head(vosDataBuff, &w8023Header, sizeof(w8023Header));

#ifdef BTAMP_TEST
  {
  // AP side will execute this.
  v_U8_t *temp_w8023Header = NULL;
  vosStatus = vos_pkt_peek_data( vosDataBuff, 0,
    &temp_w8023Header, sizeof(w8023Header) );
  }
#endif
#if 0 /*TL_DEBUG*/
  vos_pkt_get_packet_length(vosDataBuff, &usLen);
  vos_pkt_pop_head( vosDataBuff, aucData, usLen);

  WLANTL_DebugFrame(aucData, usLen);

  vos_pkt_push_head(vosDataBuff, aucData, usLen);

#endif

  *pvosStatus = VOS_STATUS_SUCCESS;

  return VOS_STATUS_SUCCESS;
}/*WLANTL_Translate80211To8023Header*/

VOS_STATUS
WLANTL_MonTranslate80211To8023Header
(
  vos_pkt_t*      vosDataBuff,
  WLANTL_CbType*  pTLCb
)
{
   v_U16_t                  usMPDUDOffset;
   v_U8_t                   ucMPDUHOffset;
   v_U8_t                   ucMPDUHLen;
   v_U16_t                  usActualHLen = 0;
   v_U16_t                  usDataStartOffset = 0;
   v_PVOID_t                aucBDHeader;
   WLANTL_8023HeaderType    w8023Header;
   WLANTL_80211HeaderType   w80211Header;
   VOS_STATUS               vosStatus;
   v_U8_t                   aucLLCHeader[WLANTL_LLC_HEADER_LEN];

   WDA_DS_PeekRxPacketInfo( vosDataBuff, (v_PVOID_t)&aucBDHeader, 0 );
   ucMPDUHOffset = (v_U8_t)WDA_GET_RX_MPDU_HEADER_OFFSET(aucBDHeader);
   usMPDUDOffset = (v_U16_t)WDA_GET_RX_MPDU_DATA_OFFSET(aucBDHeader);
   ucMPDUHLen    = (v_U8_t)WDA_GET_RX_MPDU_HEADER_LEN(aucBDHeader);
   if (usMPDUDOffset > ucMPDUHOffset)
   {
      usActualHLen = usMPDUDOffset - ucMPDUHOffset;
   }

  if ( sizeof(w80211Header) < ucMPDUHLen )
  {
     TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
       "Warning !: Check the header size for the Rx frame structure=%d received=%dn",
       sizeof(w80211Header), ucMPDUHLen));
     ucMPDUHLen = sizeof(w80211Header);
  }

  vosStatus = vos_pkt_pop_head( vosDataBuff, &w80211Header, ucMPDUHLen);
  if ( VOS_STATUS_SUCCESS != vosStatus )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL: Failed to pop 80211 header from packet %d",
                vosStatus));

     return vosStatus;
  }
  switch ( w80211Header.wFrmCtrl.fromDS )
  {
  case 0:
    if ( w80211Header.wFrmCtrl.toDS )
    {
      vos_mem_copy( w8023Header.vDA, w80211Header.vA3, VOS_MAC_ADDR_SIZE);
      vos_mem_copy( w8023Header.vSA, w80211Header.vA2, VOS_MAC_ADDR_SIZE);
      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                  "WLAN TL SoftAP: 802 3 DA %08x SA %08x",
                  w8023Header.vDA, w8023Header.vSA));
    }
    else
    {
      vos_mem_copy( w8023Header.vDA, w80211Header.vA1, VOS_MAC_ADDR_SIZE);
      vos_mem_copy( w8023Header.vSA, w80211Header.vA2, VOS_MAC_ADDR_SIZE);
    }
    break;
  case 1:
    if ( w80211Header.wFrmCtrl.toDS )
    {
      vos_mem_copy( w8023Header.vDA, w80211Header.vA1, VOS_MAC_ADDR_SIZE);
      vos_mem_copy( w8023Header.vSA, w80211Header.vA2, VOS_MAC_ADDR_SIZE);
    }
    else
    {
      vos_mem_copy( w8023Header.vDA, w80211Header.vA1, VOS_MAC_ADDR_SIZE);
      vos_mem_copy( w8023Header.vSA, w80211Header.vA3, VOS_MAC_ADDR_SIZE);
    }
    break;
  }
  if( usActualHLen > ucMPDUHLen )
  {
     usDataStartOffset = usActualHLen - ucMPDUHLen;
  }

  if ( 0 < usDataStartOffset )
  {
    vosStatus = vos_pkt_trim_head( vosDataBuff, usDataStartOffset );

    if ( VOS_STATUS_SUCCESS != vosStatus )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                  "WLAN TL: Failed to trim header from packet %d",
                  vosStatus));
      return vosStatus;
    }
  }
   // Extract the LLC header
   vosStatus = vos_pkt_pop_head( vosDataBuff, aucLLCHeader,
                                 WLANTL_LLC_HEADER_LEN);

   if ( VOS_STATUS_SUCCESS != vosStatus )
   {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                 "WLAN TL: Failed to pop LLC header from packet %d",
                 vosStatus));

      return vosStatus;
   }

   //Extract the length
   vos_mem_copy(&w8023Header.usLenType,
     &aucLLCHeader[WLANTL_LLC_HEADER_LEN - sizeof(w8023Header.usLenType)],
     sizeof(w8023Header.usLenType) );

   vos_pkt_push_head(vosDataBuff, &w8023Header, sizeof(w8023Header));
   return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANTL_FindFrameTypeBcMcUc

  DESCRIPTION
    Utility function to find whether received frame is broadcast, multicast
    or unicast.

  DEPENDENCIES
    The STA must be registered with TL before this function can be called.

  PARAMETERS

   IN
   pTLCb:          pointer to the TL's control block
   ucSTAId:        identifier of the station being processed
   vosDataBuff:    pointer to the vos buffer

   IN/OUT
    pucBcMcUc:       pointer to buffer, will contain frame type on return

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_E_BADMSG:  failed to extract info from data buffer
    VOS_STATUS_SUCCESS:   success

  SIDE EFFECTS
    None.
============================================================================*/
VOS_STATUS
WLANTL_FindFrameTypeBcMcUc
(
  WLANTL_CbType *pTLCb,
  v_U8_t        ucSTAId,
  vos_pkt_t     *vosDataBuff,
  v_U8_t        *pucBcMcUc
)
{
   VOS_STATUS    vosStatus = VOS_STATUS_SUCCESS;
   v_PVOID_t     aucBDHeader;
   v_PVOID_t     pvPeekData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

   /*------------------------------------------------------------------------
     Sanity check
    ------------------------------------------------------------------------*/
   if ((NULL == pTLCb) || 
       (NULL == vosDataBuff) || 
       (NULL == pucBcMcUc))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:Invalid parameter in WLANTL_FindFrameTypeBcMcUc"));
      return VOS_STATUS_E_INVAL;
   }

   if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
   {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Client Memory was not allocated on %s", __func__));
       return VOS_STATUS_E_FAILURE;
   }

   /*------------------------------------------------------------------------
     Extract BD header and check if valid
    ------------------------------------------------------------------------*/
   vosStatus = WDA_DS_PeekRxPacketInfo(vosDataBuff, (v_PVOID_t)&aucBDHeader, 0/*Swap BD*/ );

   if (NULL == aucBDHeader)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:WLANTL_FindFrameTypeBcMcUc - Cannot extract BD header"));
      VOS_ASSERT(0);
      return VOS_STATUS_E_BADMSG;
   }

   if ((0 == WDA_GET_RX_FT_DONE(aucBDHeader)) &&
       (0 != pTLCb->atlSTAClients[ucSTAId]->wSTADesc.ucSwFrameRXXlation))
   {
      /* Its an 802.11 frame, extract MAC address 1 */
      TLLOG2(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:WLANTL_FindFrameTypeBcMcUc - 802.11 frame, peeking Addr1"));
      vosStatus = vos_pkt_peek_data(vosDataBuff, WLANTL_MAC_ADDR_ALIGN(1), 
                                    (v_PVOID_t)&pvPeekData, VOS_MAC_ADDR_SIZE);
   }
   else
   {
      /* Its an 802.3 frame, extract Destination MAC address */
      TLLOG2(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:WLANTL_FindFrameTypeBcMcUc - 802.3 frame, peeking DA"));
      vosStatus = vos_pkt_peek_data(vosDataBuff, WLANTL_MAC_ADDR_ALIGN(0),
                                    (v_PVOID_t)&pvPeekData, VOS_MAC_ADDR_SIZE);
   }

   if (VOS_STATUS_SUCCESS != vosStatus) 
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
             "WLAN TL:WLANTL_FindFrameTypeBcMcUc - Failed to peek MAC address"));
      return vosStatus;
   }

   if (((tANI_U8 *)pvPeekData)[0] == 0xff)
   {
      *pucBcMcUc = WLANTL_FRAME_TYPE_BCAST;
   }
   else
   {
      if ((((tANI_U8 *)pvPeekData)[0] & 0x01) == 0x01)
         *pucBcMcUc = WLANTL_FRAME_TYPE_MCAST;
      else
         *pucBcMcUc = WLANTL_FRAME_TYPE_UCAST;
   }

   TLLOG2(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
          "WLAN TL:WLANTL_FindFrameTypeBcMcUc - Addr1Byte1 is: %x", 
          ((tANI_U8 *)pvPeekData)[0]));

  return VOS_STATUS_SUCCESS;
}

#if 0
#ifdef WLAN_PERF 
/*==========================================================================
  FUNCTION    WLANTL_FastHwFwdDataFrame

  DESCRIPTION 
    Fast path function to quickly forward a data frame if HAL determines BD 
    signature computed here matches the signature inside current VOSS packet. 
    If there is a match, HAL and TL fills in the swapped packet length into 
    BD header and DxE header, respectively. Otherwise, packet goes back to 
    normal (slow) path and a new BD signature would be tagged into BD in this
    VOSS packet later by the WLANHAL_FillTxBd() function.

  DEPENDENCIES 
     
  PARAMETERS 

   IN
        pvosGCtx    VOS context
        vosDataBuff Ptr to VOSS packet
        pMetaInfo   For getting frame's TID
        pStaInfo    For checking STA type
    
   OUT
        pvosStatus  returned status
        puFastFwdOK Flag to indicate whether frame could be fast forwarded
   
  RETURN VALUE
    No return.   

  SIDE EFFECTS 
  
============================================================================*/
static void
WLANTL_FastHwFwdDataFrame
( 
  v_PVOID_t     pvosGCtx,
  vos_pkt_t*    vosDataBuff,
  VOS_STATUS*   pvosStatus,
  v_U32_t*       puFastFwdOK,
  WLANTL_MetaInfoType*  pMetaInfo,
  WLAN_STADescType*  pStaInfo
 
)
{
    v_PVOID_t   pvPeekData;
    v_U8_t      ucDxEBDWLANHeaderLen = WLANTL_BD_HEADER_LEN(0) + sizeof(WLANBAL_sDXEHeaderType); 
    v_U8_t      ucIsUnicast;
    WLANBAL_sDXEHeaderType  *pDxEHeader;
    v_PVOID_t   pvBDHeader;
    v_PVOID_t   pucBuffPtr;
    v_U16_t      usPktLen;

   /*-----------------------------------------------------------------------
    Extract packet length
    -----------------------------------------------------------------------*/

    vos_pkt_get_packet_length( vosDataBuff, &usPktLen);

   /*-----------------------------------------------------------------------
    Extract MAC address
    -----------------------------------------------------------------------*/
    *pvosStatus = vos_pkt_peek_data( vosDataBuff, 
                                 WLANTL_MAC_ADDR_ALIGN(0), 
                                 (v_PVOID_t)&pvPeekData, 
                                 VOS_MAC_ADDR_SIZE );

    if ( VOS_STATUS_SUCCESS != *pvosStatus ) 
    {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                  "WLAN TL:Failed while attempting to extract MAC Addr %d", 
                  *pvosStatus));
       *pvosStatus = VOS_STATUS_E_INVAL;
       return;
    }

   /*-----------------------------------------------------------------------
    Reserve head room for DxE header, BD, and WLAN header
    -----------------------------------------------------------------------*/

    vos_pkt_reserve_head( vosDataBuff, &pucBuffPtr, 
                        ucDxEBDWLANHeaderLen );
    if ( NULL == pucBuffPtr )
    {
        TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                    "WLAN TL:No enough space in VOSS packet %p for DxE/BD/WLAN header", vosDataBuff));
       *pvosStatus = VOS_STATUS_E_INVAL;
        return;
    }
    pDxEHeader = (WLANBAL_sDXEHeaderType  *)pucBuffPtr;
    pvBDHeader = (v_PVOID_t) &pDxEHeader[1];

    /* UMA Tx acceleration is enabled. 
     * UMA would help convert frames to 802.11, fill partial BD fields and 
     * construct LLC header. To further accelerate this kind of frames,
     * HAL would attempt to reuse the BD descriptor if the BD signature 
     * matches to the saved BD descriptor.
     */
     if(pStaInfo->wSTAType == WLAN_STA_IBSS)
        ucIsUnicast = !(((tANI_U8 *)pvPeekData)[0] & 0x01);
     else
        ucIsUnicast = 1;
 
     *puFastFwdOK = (v_U32_t) WLANHAL_TxBdFastFwd(pvosGCtx, pvPeekData, pMetaInfo->ucTID, ucIsUnicast, pvBDHeader, usPktLen );
    
      /* Can't be fast forwarded. Trim the VOS head back to original location. */
      if(! *puFastFwdOK){
          vos_pkt_trim_head(vosDataBuff, ucDxEBDWLANHeaderLen);
      }else{
        /* could be fast forwarded. Now notify BAL DxE header filling could be completely skipped
         */
        v_U32_t uPacketSize = WLANTL_BD_HEADER_LEN(0) + usPktLen;
        vos_pkt_set_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_BAL, 
                       (v_PVOID_t)uPacketSize);
        pDxEHeader->size  = SWAP_ENDIAN_UINT32(uPacketSize);
      }
     *pvosStatus = VOS_STATUS_SUCCESS;
      return;
}
#endif /*WLAN_PERF*/
#endif

#if 0
/*==========================================================================
   FUNCTION    WLANTL_PrepareBDHeader

  DESCRIPTION
    Inline function for preparing BD header before HAL processing.

  DEPENDENCIES
    Just notify HAL that suspend in TL is complete.

  PARAMETERS

   IN
    vosDataBuff:      vos data buffer
    ucDisableFrmXtl:  is frame xtl disabled

   OUT
    ppvBDHeader:      it will contain the BD header
    pvDestMacAdddr:   it will contain the destination MAC address
    pvosStatus:       status of the combined processing
    pusPktLen:        packet len.

  RETURN VALUE
    No return.

  SIDE EFFECTS

============================================================================*/
void
WLANTL_PrepareBDHeader
(
  vos_pkt_t*      vosDataBuff,
  v_PVOID_t*      ppvBDHeader,
  v_MACADDR_t*    pvDestMacAdddr,
  v_U8_t          ucDisableFrmXtl,
  VOS_STATUS*     pvosStatus,
  v_U16_t*        pusPktLen,
  v_U8_t          ucQosEnabled,
  v_U8_t          ucWDSEnabled,
  v_U8_t          extraHeadSpace
)
{
  v_U8_t      ucHeaderOffset;
  v_U8_t      ucHeaderLen;
  v_U8_t      ucBDHeaderLen = WLANTL_BD_HEADER_LEN(ucDisableFrmXtl);

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
  /*-------------------------------------------------------------------------
    Get header pointer from VOSS
    !!! make sure reserve head zeros out the memory
   -------------------------------------------------------------------------*/
  vos_pkt_get_packet_length( vosDataBuff, pusPktLen);

  if ( WLANTL_MAC_HEADER_LEN(ucDisableFrmXtl) > *pusPktLen )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL: Length of the packet smaller than expected network"
               " header %d", *pusPktLen ));

    *pvosStatus = VOS_STATUS_E_INVAL;
    return;
  }

  vos_pkt_reserve_head( vosDataBuff, ppvBDHeader,
                        ucBDHeaderLen );
  if ( NULL == *ppvBDHeader )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:VOSS packet corrupted on Attach BD header"));
    *pvosStatus = VOS_STATUS_E_INVAL;
    return;
  }

  /*-----------------------------------------------------------------------
    Extract MAC address
   -----------------------------------------------------------------------*/
  {
   v_SIZE_t usMacAddrSize = VOS_MAC_ADDR_SIZE;
   *pvosStatus = vos_pkt_extract_data( vosDataBuff,
                                     ucBDHeaderLen +
                                     WLANTL_MAC_ADDR_ALIGN(ucDisableFrmXtl),
                                     (v_PVOID_t)pvDestMacAdddr,
                                     &usMacAddrSize );
  }
  if ( VOS_STATUS_SUCCESS != *pvosStatus )
  {
     TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "WLAN TL:Failed while attempting to extract MAC Addr %d",
                *pvosStatus));
  }
  else
  {
    /*---------------------------------------------------------------------
        Fill MPDU info fields:
          - MPDU data start offset
          - MPDU header start offset
          - MPDU header length
          - MPDU length - this is a 16b field - needs swapping
    --------------------------------------------------------------------*/
    ucHeaderOffset = ucBDHeaderLen;
    ucHeaderLen    = WLANTL_MAC_HEADER_LEN(ucDisableFrmXtl);

    if ( 0 != ucDisableFrmXtl )
    {
      if ( 0 != ucQosEnabled )
      {
        ucHeaderLen += WLANTL_802_11_HEADER_QOS_CTL;
      }

      // Similar to Qos we need something for WDS format !
      if ( ucWDSEnabled != 0 )
      {
        // If we have frame translation enabled
        ucHeaderLen    += WLANTL_802_11_HEADER_ADDR4_LEN;
      }
      if ( extraHeadSpace != 0 )
      {
        // Decrease the packet length with the extra padding after the header
        *pusPktLen = *pusPktLen - extraHeadSpace;
      }
    }

    WLANHAL_TX_BD_SET_MPDU_HEADER_LEN( *ppvBDHeader, ucHeaderLen);
    WLANHAL_TX_BD_SET_MPDU_HEADER_OFFSET( *ppvBDHeader, ucHeaderOffset);
    WLANHAL_TX_BD_SET_MPDU_DATA_OFFSET( *ppvBDHeader,
                                          ucHeaderOffset + ucHeaderLen + extraHeadSpace);
    WLANHAL_TX_BD_SET_MPDU_LEN( *ppvBDHeader, *pusPktLen);

    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                "WLAN TL: VALUES ARE HLen=%x Hoff=%x doff=%x len=%x ex=%d",
                ucHeaderLen, ucHeaderOffset, 
                (ucHeaderOffset + ucHeaderLen + extraHeadSpace), 
                *pusPktLen, extraHeadSpace));
  }/* if peek MAC success*/

}/* WLANTL_PrepareBDHeader */
#endif

//THIS IS A HACK AND NEEDS TO BE FIXED FOR CONCURRENCY
/*==========================================================================
  FUNCTION    WLAN_TLGetNextTxIds

  DESCRIPTION
    Gets the next station and next AC in the list that should be served by the TL.

    Multiple Station Scheduling and TL queue management. 

    4 HDD BC/MC data packet queue status is specified as Station 0's status. Weights used
    in WFQ algorith are initialized in WLANTL_OPEN and contained in tlConfigInfo field.
    Each station has fields of ucPktPending and AC mask to tell whether a AC has traffic
    or not.
      
    Stations are served in a round-robin fashion from highest priority to lowest priority.
    The number of round-robin times of each prioirty equals to the WFQ weights and differetiates
    the traffic of different prioirty. As such, stations can not provide low priority packets if
    high priority packets are all served.

  DEPENDENCIES

  PARAMETERS

   IN
   pvosGCtx:     pointer to the global vos context; a handle to TL's
                 control block can be extracted from its context

   OUT
   pucSTAId:    Station ID

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_SUCCESS:   Everything is good

  SIDE EFFECTS
   
   TL context contains currently served station ID in ucCurrentSTA field, currently served AC
   in uCurServedAC field, and unserved weights of current AC in uCurLeftWeight.
   When existing from the function, these three fields are changed accordingly.

============================================================================*/
VOS_STATUS
WLAN_TLAPGetNextTxIds
(
  v_PVOID_t    pvosGCtx,
  v_U8_t*      pucSTAId
)
{
  WLANTL_CbType*  pTLCb;
  v_U8_t          ucACFilter = 1;
  v_U8_t          ucNextSTA ; 
  v_BOOL_t        isServed = TRUE;  //current round has find a packet or not
  v_U8_t          ucACLoopNum = WLANTL_AC_HIGH_PRIO + 1; //number of loop to go
  v_U8_t          uFlowMask; // TX FlowMask from WDA
  uint8           ucACMask; 
  uint8           i = 0; 
  /*------------------------------------------------------------------------
    Extract TL control block
  ------------------------------------------------------------------------*/
  //ENTER();

  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL:Invalid TL pointer from pvosGCtx on WLAN_TLAPGetNextTxIds"));
    return VOS_STATUS_E_FAULT;
  }

  if ( VOS_STATUS_SUCCESS != WDA_DS_GetTxFlowMask( pvosGCtx, &uFlowMask ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL:Failed to retrieve Flow control mask from WDA"));
    return VOS_STATUS_E_FAULT;
  }

  /* The flow mask does not differentiate between different ACs/Qs
   * since we use a single dxe channel for all ACs/Qs, hence it is
   * enough to check that there are dxe resources on data channel
   */
  uFlowMask &= WLANTL_DATA_FLOW_MASK;

  if (0 == uFlowMask)
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL: No resources to send packets"));

    // Setting STA Id to invalid if mask is 0
    *pucSTAId = WLAN_MAX_STA_COUNT;
    return VOS_STATUS_E_FAULT;
  }

  ucNextSTA = pTLCb->ucCurrentSTA;

  ++ucNextSTA;

  if ( WLAN_MAX_STA_COUNT <= ucNextSTA )
  {
    //one round is done.
    ucNextSTA = 0;
    pTLCb->ucCurLeftWeight--;
    isServed = FALSE;
    if ( 0 == pTLCb->ucCurLeftWeight )
    {
      //current prioirty is done
      if ( WLANTL_AC_BK == (WLANTL_ACEnumType)pTLCb->uCurServedAC )
      {
        //end of current VO, VI, BE, BK loop. Reset priority.
        pTLCb->uCurServedAC = WLANTL_AC_HIGH_PRIO;
      }
      else 
      {
        pTLCb->uCurServedAC --;
      }

      pTLCb->ucCurLeftWeight =  pTLCb->tlConfigInfo.ucAcWeights[pTLCb->uCurServedAC];
 
    } // (0 == pTLCb->ucCurLeftWeight)
  } //( WLAN_MAX_STA_COUNT == ucNextSTA )

  //decide how many loops to go. if current loop is partial, do one extra to make sure
  //we cover every station
  if ((1 == pTLCb->ucCurLeftWeight) && (ucNextSTA != 0))
  {
    ucACLoopNum ++; // now is 5 loops
  }

  /* Start with highest priority. ucNextSTA, pTLCb->uCurServedAC, pTLCb->ucCurLeftWeight
     all have previous values.*/
  for (; ucACLoopNum > 0;  ucACLoopNum--)
  {

    ucACFilter = 1 << pTLCb->uCurServedAC;

    // pTLCb->ucCurLeftWeight keeps previous results.
    for (; (pTLCb->ucCurLeftWeight > 0) ; pTLCb->ucCurLeftWeight-- )
    {

      for ( ; ucNextSTA < WLAN_MAX_STA_COUNT; ucNextSTA ++ )
      {
        if(NULL == pTLCb->atlSTAClients[ucNextSTA])
        {
            continue;
        }
        WLAN_TL_AC_ARRAY_2_MASK (pTLCb->atlSTAClients[ucNextSTA], ucACMask, i);

        if ( (0 == pTLCb->atlSTAClients[ucNextSTA]->ucExists) ||
             ((0 == pTLCb->atlSTAClients[ucNextSTA]->ucPktPending) && !(ucACMask)) ||
             (0 == (ucACMask & ucACFilter)) )

        {
          //current station does not exist or have any packet to serve.
          continue;
        }

        if (WLANTL_STA_AUTHENTICATED != pTLCb->atlSTAClients[ucNextSTA]->tlState)
        {
          TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "%s Sta %d not in auth state so skipping it.",
                 __func__, ucNextSTA));
          continue;
        }

        //go to next station if current station can't send due to flow control
        //Station is allowed to send when it is not in LWM mode. When station is in LWM mode,
        //station is allowed to send only after FW reports FW memory is below threshold and on-fly
        //packets are less then allowed value
        if ( (TRUE == pTLCb->atlSTAClients[ucNextSTA]->ucLwmModeEnabled) &&
             ((FALSE == pTLCb->atlSTAClients[ucNextSTA]->ucLwmEventReported) ||
                 (0 < pTLCb->atlSTAClients[ucNextSTA]->uBuffThresholdMax))
           )
        {
          continue;
        }


        // Find a station. Weight is updated already.
        *pucSTAId = ucNextSTA;
        pTLCb->ucCurrentSTA = ucNextSTA;
        pTLCb->atlSTAClients[*pucSTAId]->ucCurrentAC = pTLCb->uCurServedAC;
  
        TLLOG4(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
                   " TL serve one station AC: %d  W: %d StaId: %d",
                   pTLCb->uCurServedAC, pTLCb->ucCurLeftWeight, pTLCb->ucCurrentSTA ));
      
        return VOS_STATUS_SUCCESS;
      } //STA loop

      ucNextSTA = 0;
      if ( FALSE == isServed )
      {
        //current loop finds no packet.no need to repeat for the same priority
        break;
      }
      //current loop is partial loop. go for one more loop.
      isServed = FALSE;

    } //Weight loop

    if (WLANTL_AC_BK == pTLCb->uCurServedAC)
    {
      pTLCb->uCurServedAC = WLANTL_AC_HIGH_PRIO;
    }
    else
    {
      pTLCb->uCurServedAC--;
    }
    pTLCb->ucCurLeftWeight =  pTLCb->tlConfigInfo.ucAcWeights[pTLCb->uCurServedAC];

  }// AC loop

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                   " TL can't find one station to serve" ));

  pTLCb->uCurServedAC = WLANTL_AC_BK;
  pTLCb->ucCurLeftWeight = 1;
  //invalid number will be captured by caller
  pTLCb->ucCurrentSTA = WLAN_MAX_STA_COUNT; 

  *pucSTAId = pTLCb->ucCurrentSTA;
  return VOS_STATUS_E_FAULT;
}


/*==========================================================================
  FUNCTION    WLAN_TLGetNextTxIds

  DESCRIPTION
    Gets the next station and next AC in the list

  DEPENDENCIES

  PARAMETERS

   IN
   pvosGCtx:     pointer to the global vos context; a handle to TL's
                 control block can be extracted from its context

   OUT
   pucSTAId:    Station ID


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLAN_TLGetNextTxIds
(
  v_PVOID_t    pvosGCtx,
  v_U8_t*      pucSTAId
)
{
  WLANTL_CbType*  pTLCb;
  v_U8_t          ucNextAC;
  v_U8_t          ucNextSTA; 
  v_U8_t          ucCount; 
  v_U8_t          uFlowMask; // TX FlowMask from WDA
  v_U8_t          ucACMask = 0;
  v_U8_t          i = 0; 

  tBssSystemRole systemRole; //RG HACK to be removed
  tpAniSirGlobal pMac;

  pMac = vos_get_context(VOS_MODULE_ID_PE, pvosGCtx);
  if ( NULL == pMac )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid pMac", __func__));
    return VOS_STATUS_E_FAULT;
  }

  systemRole = wdaGetGlobalSystemRole(pMac);

  /*------------------------------------------------------------------------
    Extract TL control block
  ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL:Invalid TL pointer from pvosGCtx on WLAN_TLGetNextTxIds"));
    return VOS_STATUS_E_FAULT;
  }

#ifdef FEATURE_WLAN_TDLS
  if ((eSYSTEM_AP_ROLE == systemRole) ||
      (eSYSTEM_STA_IN_IBSS_ROLE == systemRole) ||
      (vos_concurrent_open_sessions_running()) || pTLCb->ucTdlsPeerCount)
#else
  if ((eSYSTEM_AP_ROLE == systemRole) ||
      (eSYSTEM_STA_IN_IBSS_ROLE == systemRole) ||
      (vos_concurrent_open_sessions_running()))
#endif
  {
    return WLAN_TLAPGetNextTxIds(pvosGCtx,pucSTAId);
  }


  if ( VOS_STATUS_SUCCESS != WDA_DS_GetTxFlowMask( pvosGCtx, &uFlowMask ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL:Failed to retrieve Flow control mask from WDA"));
    return VOS_STATUS_E_FAULT;
  }

  /* The flow mask does not differentiate between different ACs/Qs
   * since we use a single dxe channel for all ACs/Qs, hence it is
   * enough to check that there are dxe resources on data channel
   */
  uFlowMask &= WLANTL_DATA_FLOW_MASK;

  if (0 == uFlowMask)
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL: No resources to send packets"));

    // Setting STA id to invalid if mask is 0
    *pucSTAId = WLAN_MAX_STA_COUNT;
    return VOS_STATUS_E_FAULT;
  }

  /*STA id - no priority yet implemented */
  /*-----------------------------------------------------------------------
    Choose the next STA for tx - for now go in a round robin fashion
    through all the stations that have pending packets     
  -------------------------------------------------------------------------*/
  ucNextSTA = pTLCb->ucCurrentSTA;
  
  pTLCb->ucCurrentSTA = WLAN_MAX_STA_COUNT; 
  for ( ucCount = 0; 
        ucCount < WLAN_MAX_STA_COUNT;
        ucCount++ )
  {
    ucNextSTA = ( (ucNextSTA+1) >= WLAN_MAX_STA_COUNT )?0:(ucNextSTA+1);
    if(NULL == pTLCb->atlSTAClients[ucNextSTA])
    {
        continue;
    }
    if (( pTLCb->atlSTAClients[ucNextSTA]->ucExists ) &&
        ( pTLCb->atlSTAClients[ucNextSTA]->ucPktPending ))
    {
      if (WLANTL_STA_AUTHENTICATED == pTLCb->atlSTAClients[ucNextSTA]->tlState)
      {
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "STA ID: %d on WLAN_TLGetNextTxIds", *pucSTAId));
        pTLCb->ucCurrentSTA = ucNextSTA;
        break;
      }
      else
      {
        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s Sta %d is not in auth state, skipping this sta.",
               __func__, ucNextSTA));
      }
    }
  }

  *pucSTAId = pTLCb->ucCurrentSTA;

  if ( WLANTL_STA_ID_INVALID( *pucSTAId ) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
      "WLAN TL:No station registered with TL at this point"));

    return VOS_STATUS_E_FAULT;

  }

  /*Convert the array to a mask for easier operation*/
  WLAN_TL_AC_ARRAY_2_MASK( pTLCb->atlSTAClients[*pucSTAId], ucACMask, i);
  
  if ( 0 == ucACMask )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
      "WLAN TL: Mask 0 "
      "STA ID: %d on WLAN_TLGetNextTxIds", *pucSTAId));

     /*setting STA id to invalid if mask is 0*/
     *pucSTAId = WLAN_MAX_STA_COUNT;

     return VOS_STATUS_E_FAULT;
  }

  /*-----------------------------------------------------------------------
    AC is updated whenever a packet is fetched from HDD -> the current
    weight of such an AC cannot be 0 -> in this case TL is expected to
    exit this function at this point during the main Tx loop
  -----------------------------------------------------------------------*/
  if ( 0 < pTLCb->atlSTAClients[*pucSTAId]->ucCurrentWeight  )
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                  "WLAN TL: Maintaining serviced AC to: %d for Weight: %d",
                  pTLCb->atlSTAClients[*pucSTAId]->ucCurrentAC ,
                  pTLCb->atlSTAClients[*pucSTAId]->ucCurrentWeight));
    return VOS_STATUS_SUCCESS;
  }

  /*-----------------------------------------------------------------------
     Choose highest priority AC - !!! optimize me
  -----------------------------------------------------------------------*/
  ucNextAC = pTLCb->atlSTAClients[*pucSTAId]->ucCurrentAC;
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "Next AC: %d", ucNextAC));

  while ( 0 != ucACMask )
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             " AC Mask: %d Next: %d Res : %d",
               ucACMask, ( 1 << ucNextAC ), ( ucACMask & ( 1 << ucNextAC ))));

    if ( 0 != ( ucACMask & ( 1 << ucNextAC )))
    {
       pTLCb->atlSTAClients[*pucSTAId]->ucCurrentAC     =
                                   (WLANTL_ACEnumType)ucNextAC;
       pTLCb->atlSTAClients[*pucSTAId]->ucCurrentWeight =
                       pTLCb->tlConfigInfo.ucAcWeights[ucNextAC];

        TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                  "WLAN TL: Switching serviced AC to: %d with Weight: %d",
                  pTLCb->atlSTAClients[*pucSTAId]->ucCurrentAC ,
                  pTLCb->atlSTAClients[*pucSTAId]->ucCurrentWeight));
       break;
    }

    if (ucNextAC == WLANTL_AC_BK)
        ucNextAC = WLANTL_AC_HIGH_PRIO;
    else
        ucNextAC--;

    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "Next AC %d", ucNextAC));
  }

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             " C AC: %d C W: %d",
             pTLCb->atlSTAClients[*pucSTAId]->ucCurrentAC,
             pTLCb->atlSTAClients[*pucSTAId]->ucCurrentWeight));

  return VOS_STATUS_SUCCESS;
}/* WLAN_TLGetNextTxIds */



/*==========================================================================
      DEFAULT HANDLERS: Registered at initialization with TL
  ==========================================================================*/

/*==========================================================================

  FUNCTION    WLANTL_MgmtFrmRxDefaultCb

  DESCRIPTION
    Default Mgmt Frm rx callback: asserts all the time. If this function gets
    called  it means there is no registered rx cb pointer for Mgmt Frm.

  DEPENDENCIES

  PARAMETERS
    Not used.

  RETURN VALUE

   VOS_STATUS_E_FAILURE: Always FAILURE.

============================================================================*/
VOS_STATUS
WLANTL_MgmtFrmRxDefaultCb
(
  v_PVOID_t  pvosGCtx,
  v_PVOID_t  vosBuff
)
{
  if ( NULL != vosBuff )
  {
    TLLOGP(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
             "WLAN TL:Fatal failure: No registered Mgmt Frm client on pkt RX"));
    /* Drop packet */
    vos_pkt_return_packet((vos_pkt_t *)vosBuff);
  }

      TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLAN TL: No registered Mgmt Frm client on pkt RX. Load/Unload in progress, Ignore"));

  return VOS_STATUS_E_FAILURE;
}/*WLANTL_MgmtFrmRxDefaultCb*/

/*==========================================================================

  FUNCTION   WLANTL_BAPRxDefaultCb

  DESCRIPTION
    Default BAP rx callback: asserts all the time. If this function gets
    called  it means there is no registered rx cb pointer for BAP.

  DEPENDENCIES

  PARAMETERS
    Not used.

  RETURN VALUE

   VOS_STATUS_E_FAILURE: Always FAILURE.

============================================================================*/
VOS_STATUS
WLANTL_BAPRxDefaultCb
(
  v_PVOID_t  pvosGCtx,
  vos_pkt_t*       vosDataBuff,
  WLANTL_BAPFrameEnumType frameType
)
{
  TLLOGP(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
             "WLAN TL:Fatal failure: No registered BAP client on BAP pkt RX"));
#ifndef BTAMP_TEST
  VOS_ASSERT(0);
#endif
  return VOS_STATUS_E_FAILURE;
}/*WLANTL_MgmtFrmRxDefaultCb*/

/*==========================================================================

  FUNCTION    WLANTL_STARxDefaultCb

  DESCRIPTION
    Default STA rx callback: asserts all the time. If this function gets
    called  it means there is no registered rx cb pointer for station.
    (Mem corruption most likely, it should never happen)

  DEPENDENCIES

  PARAMETERS
    Not used.

  RETURN VALUE

    VOS_STATUS_E_FAILURE: Always FAILURE.

============================================================================*/
VOS_STATUS
WLANTL_STARxDefaultCb
(
  v_PVOID_t               pvosGCtx,
  vos_pkt_t*              vosDataBuff,
  v_U8_t                  ucSTAId,
  WLANTL_RxMetaInfoType*  pRxMetaInfo
)
{
  TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
       "WLAN TL: No registered STA client rx cb for STAID: %d dropping pkt",
               ucSTAId));
  vos_pkt_return_packet(vosDataBuff);
  return VOS_STATUS_SUCCESS;
}/*WLANTL_MgmtFrmRxDefaultCb*/


/*==========================================================================

  FUNCTION    WLANTL_STAFetchPktDefaultCb

  DESCRIPTION
    Default fetch callback: asserts all the time. If this function gets
    called  it means there is no registered fetch cb pointer for station.
    (Mem corruption most likely, it should never happen)

  DEPENDENCIES

  PARAMETERS
    Not used.

  RETURN VALUE

    VOS_STATUS_E_FAILURE: Always FAILURE.

============================================================================*/
VOS_STATUS
WLANTL_STAFetchPktDefaultCb
(
  v_PVOID_t              pvosGCtx,
  v_U8_t*                pucSTAId,
  WLANTL_ACEnumType      ucAC,
  vos_pkt_t**            vosDataBuff,
  WLANTL_MetaInfoType*   tlMetaInfo
)
{
  TLLOGP(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
             "WLAN TL:Fatal failure: No registered STA client on data pkt RX"));
  VOS_ASSERT(0);
  return VOS_STATUS_E_FAILURE;
}/*WLANTL_MgmtFrmRxDefaultCb*/

/*==========================================================================

  FUNCTION    WLANTL_TxCompDefaultCb

  DESCRIPTION
    Default tx complete handler. It will release the completed pkt to
    prevent memory leaks.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to
                    TL/HAL/PE/BAP/HDD control block can be extracted from
                    its context
    vosDataBuff:   pointer to the VOSS data buffer that was transmitted
    wTxSTAtus:      status of the transmission


  RETURN VALUE
    The result code associated with performing the operation; please
    check vos_pkt_return_packet for possible error codes.

    Please check  vos_pkt_return_packet API for possible return values.

============================================================================*/
VOS_STATUS
WLANTL_TxCompDefaultCb
(
 v_PVOID_t      pvosGCtx,
 vos_pkt_t*     vosDataBuff,
 VOS_STATUS     wTxSTAtus
)
{
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
         "WLAN TL:TXComp not registered, releasing pkt to prevent mem leak"));
  return vos_pkt_return_packet(vosDataBuff);
}/*WLANTL_TxCompDefaultCb*/


/*==========================================================================
      Cleanup functions
  ==========================================================================*/

/*==========================================================================

  FUNCTION    WLANTL_CleanCB

  DESCRIPTION
    Cleans TL control block

  DEPENDENCIES

  PARAMETERS

    IN
    pTLCb:       pointer to TL's control block
    ucEmpty:     set if TL has to clean up the queues and release pedning pkts

  RETURN VALUE
    The result code associated with performing the operation

     VOS_STATUS_E_INVAL:   invalid input parameters
     VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_CleanCB
(
  WLANTL_CbType*  pTLCb,
  v_U8_t      ucEmpty
)
{
  v_U8_t ucIndex;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*-------------------------------------------------------------------------
    Sanity check
   -------------------------------------------------------------------------*/
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_CleanCB"));
    return VOS_STATUS_E_INVAL;
  }

  /* number of packets sent to BAL waiting for tx complete confirmation */
  pTLCb->usPendingTxCompleteCount = 0;

  /* global suspend flag */
   vos_atomic_set_U8( &pTLCb->ucTxSuspended, 1);

  /* resource flag */
  pTLCb->uResCount = 0;


  /*-------------------------------------------------------------------------
    Client stations
   -------------------------------------------------------------------------*/
  for ( ucIndex = 0; ucIndex < WLAN_MAX_STA_COUNT ; ucIndex++)
  {
    if(NULL != pTLCb->atlSTAClients[ucIndex])
    {
        WLANTL_CleanSTA( pTLCb->atlSTAClients[ucIndex], ucEmpty);
    }
  }

  /*-------------------------------------------------------------------------
    Management Frame client
   -------------------------------------------------------------------------*/
  pTLCb->tlMgmtFrmClient.ucExists = 0;

  if ( ( 0 != ucEmpty) &&
       ( NULL != pTLCb->tlMgmtFrmClient.vosPendingDataBuff ))
  {
    vos_pkt_return_packet(pTLCb->tlMgmtFrmClient.vosPendingDataBuff);
  }

  pTLCb->tlMgmtFrmClient.vosPendingDataBuff  = NULL;

  /* set to a default cb in order to prevent constant checking for NULL */
  pTLCb->tlMgmtFrmClient.pfnTlMgmtFrmRx = WLANTL_MgmtFrmRxDefaultCb;

  /*-------------------------------------------------------------------------
    BT AMP client
   -------------------------------------------------------------------------*/
  pTLCb->tlBAPClient.ucExists = 0;

  if (( 0 != ucEmpty) &&
      ( NULL != pTLCb->tlBAPClient.vosPendingDataBuff ))
  {
    vos_pkt_return_packet(pTLCb->tlBAPClient.vosPendingDataBuff);
  }
  
  if (( 0 != ucEmpty) &&
      ( NULL != pTLCb->vosDummyBuf ))
  {
    vos_pkt_return_packet(pTLCb->vosDummyBuf);
  }

  pTLCb->tlBAPClient.vosPendingDataBuff  = NULL;

  pTLCb->vosDummyBuf = NULL;
  pTLCb->vosTempBuf  = NULL;
  pTLCb->ucCachedSTAId = WLAN_MAX_STA_COUNT;

  /* set to a default cb in order to prevent constant checking for NULL */
  pTLCb->tlBAPClient.pfnTlBAPRx = WLANTL_BAPRxDefaultCb;

  pTLCb->ucRegisteredStaId = WLAN_MAX_STA_COUNT;

  return VOS_STATUS_SUCCESS;

}/* WLANTL_CleanCB*/

/*==========================================================================

  FUNCTION    WLANTL_CleanSTA

  DESCRIPTION
    Cleans a station control block.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    ucEmpty:        if set the queues and pending pkts will be emptyed

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:   invalid input parameters
    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_CleanSTA
(
  WLANTL_STAClientType*  ptlSTAClient,
  v_U8_t             ucEmpty
)
{
  v_U8_t  ucIndex;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*-------------------------------------------------------------------------
    Sanity check
   -------------------------------------------------------------------------*/
  if ( NULL == ptlSTAClient )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_CleanSTA"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Clear station from TL
   ------------------------------------------------------------------------*/
  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL: Clearing STA Client ID: %d, Empty flag: %d",
             ptlSTAClient->wSTADesc.ucSTAId, ucEmpty ));

  ptlSTAClient->pfnSTARx          = WLANTL_STARxDefaultCb;
  ptlSTAClient->pfnSTATxComp      = WLANTL_TxCompDefaultCb;
  ptlSTAClient->pfnSTAFetchPkt    = WLANTL_STAFetchPktDefaultCb;

  ptlSTAClient->tlState           = WLANTL_STA_INIT;
  ptlSTAClient->tlPri             = WLANTL_STA_PRI_NORMAL;

  vos_zero_macaddr( &ptlSTAClient->wSTADesc.vSTAMACAddress );
  vos_zero_macaddr( &ptlSTAClient->wSTADesc.vBSSIDforIBSS );
  vos_zero_macaddr( &ptlSTAClient->wSTADesc.vSelfMACAddress );

  ptlSTAClient->wSTADesc.ucSTAId  = 0;
  ptlSTAClient->wSTADesc.wSTAType = WLAN_STA_MAX;

  ptlSTAClient->wSTADesc.ucQosEnabled     = 0;
  ptlSTAClient->wSTADesc.ucAddRmvLLC      = 0;
  ptlSTAClient->wSTADesc.ucSwFrameTXXlation = 0;
  ptlSTAClient->wSTADesc.ucSwFrameRXXlation = 0;
  ptlSTAClient->wSTADesc.ucProtectedFrame = 0;

  /*-------------------------------------------------------------------------
    AMSDU information for the STA
   -------------------------------------------------------------------------*/
  if ( ( 0 != ucEmpty ) &&
       ( NULL != ptlSTAClient->vosAMSDUChainRoot ))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
               "WLAN TL:Non NULL vosAMSDUChainRoot on WLANTL_CleanSTA, "
               "suspecting a memory corruption"));

  }

  ptlSTAClient->vosAMSDUChain     = NULL;
  ptlSTAClient->vosAMSDUChainRoot = NULL;

  vos_mem_zero( (v_PVOID_t)ptlSTAClient->aucMPDUHeader,
                 WLANTL_MPDU_HEADER_LEN);
  ptlSTAClient->ucMPDUHeaderLen    = 0;

  /*-------------------------------------------------------------------------
    Reordering information for the STA
   -------------------------------------------------------------------------*/
  for ( ucIndex = 0; ucIndex < WLAN_MAX_TID ; ucIndex++)
  {
    if(0 == ptlSTAClient->atlBAReorderInfo[ucIndex].ucExists)
    {
      continue;
    }
    if(NULL != ptlSTAClient->atlBAReorderInfo[ucIndex].reorderBuffer)
    {
      ptlSTAClient->atlBAReorderInfo[ucIndex].reorderBuffer->isAvailable = VOS_TRUE;
      memset(&ptlSTAClient->atlBAReorderInfo[ucIndex].reorderBuffer->arrayBuffer[0], 0, WLANTL_MAX_WINSIZE * sizeof(v_PVOID_t));
    }
    vos_timer_destroy(&ptlSTAClient->atlBAReorderInfo[ucIndex].agingTimer);
    memset(&ptlSTAClient->atlBAReorderInfo[ucIndex], 0, sizeof(WLANTL_BAReorderType));
  }

  /*-------------------------------------------------------------------------
     QOS information for the STA
    -------------------------------------------------------------------------*/
   ptlSTAClient->ucCurrentAC     = WLANTL_AC_HIGH_PRIO;
   ptlSTAClient->ucCurrentWeight = 0;
   ptlSTAClient->ucServicedAC    = WLANTL_AC_BK;

   vos_mem_zero( ptlSTAClient->aucACMask, sizeof(ptlSTAClient->aucACMask));
   vos_mem_zero( &ptlSTAClient->wUAPSDInfo, sizeof(ptlSTAClient->wUAPSDInfo));


  /*--------------------------------------------------------------------
    Stats info
    --------------------------------------------------------------------*/
   vos_mem_zero( ptlSTAClient->auRxCount,
                 sizeof(ptlSTAClient->auRxCount[0])* WLAN_MAX_TID);
   vos_mem_zero( ptlSTAClient->auTxCount,
                 sizeof(ptlSTAClient->auTxCount[0])* WLAN_MAX_TID);
   ptlSTAClient->rssiAvg = 0;

   /*Tx not suspended and station fully registered*/
   vos_atomic_set_U8( &ptlSTAClient->ucTxSuspended, 0);
   vos_atomic_set_U8( &ptlSTAClient->ucNoMoreData, 1);

  if ( 0 == ucEmpty )
  {
    ptlSTAClient->wSTADesc.ucUcastSig       = WLAN_TL_INVALID_U_SIG;
    ptlSTAClient->wSTADesc.ucBcastSig       = WLAN_TL_INVALID_B_SIG;
  }

  ptlSTAClient->ucExists       = 0;

  /*--------------------------------------------------------------------
    Statistics info 
    --------------------------------------------------------------------*/
  memset(&ptlSTAClient->trafficStatistics,
         0,
         sizeof(WLANTL_TRANSFER_STA_TYPE));

  /*fix me!!: add new values from the TL Cb for cleanup */
  return VOS_STATUS_SUCCESS;
}/* WLANTL_CleanSTA */


/*==========================================================================
  FUNCTION    WLANTL_EnableUAPSDForAC

  DESCRIPTION
   Called by HDD to enable UAPSD. TL in turn calls WDA API to enable the
   logic in FW/SLM to start sending trigger frames. Previously TL had the
   trigger frame logic which later moved down to FW. Hence
   HDD -> TL -> WDA -> FW call flow.

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    ucSTAId:        station Id
    ucAC:           AC for which U-APSD is being enabled
    ucTid:          TID for which U-APSD is setup
    ucUP:           used to place in the trigger frame generation
    ucServiceInt:   service interval used by TL to send trigger frames
    ucSuspendInt:   suspend interval used by TL to determine that an
                    app is idle and should start sending trigg frms less often
    wTSDir:         direction of TSpec

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_EnableUAPSDForAC
(
  v_PVOID_t          pvosGCtx,
  v_U8_t             ucSTAId,
  WLANTL_ACEnumType  ucAC,
  v_U8_t             ucTid,
  v_U8_t             ucUP,
  v_U32_t            uServiceInt,
  v_U32_t            uSuspendInt,
  WLANTL_TSDirType   wTSDir
)
{

  WLANTL_CbType*      pTLCb      = NULL;
  VOS_STATUS          vosStatus   = VOS_STATUS_SUCCESS;
  tUapsdInfo          halUAPSDInfo; 
 /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if (( NULL == pTLCb ) || WLANTL_STA_ID_INVALID( ucSTAId )
      ||   WLANTL_AC_INVALID(ucAC))
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid input params on WLANTL_EnableUAPSDForAC"
               " TL: %p  STA: %d  AC: %d",
               pTLCb, ucSTAId, ucAC));
    return VOS_STATUS_E_FAULT;
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  /*Set this flag in order to remember that this is a trigger enabled AC*/
  pTLCb->atlSTAClients[ucSTAId]->wUAPSDInfo[ucAC].ucSet = 1;

#ifdef FEATURE_WLAN_TDLS
  if(pTLCb->atlSTAClients[ucSTAId]->wSTADesc.wSTAType != WLAN_STA_TDLS)
#endif
  {
    if( 0 == uServiceInt )
    {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Input params on WLANTL_EnableUAPSDForAC"
               " SI: %d", uServiceInt ));
    }

    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:Enabling U-APSD in FW for STA: %d AC: %d SI: %d SPI: %d "
               "DI: %d",
               ucSTAId, ucAC, uServiceInt, uSuspendInt,
               pTLCb->tlConfigInfo.uDelayedTriggerFrmInt));

    /*Save all info for HAL*/
    halUAPSDInfo.staidx         = ucSTAId;
    halUAPSDInfo.ac             = ucAC;
    halUAPSDInfo.up             = ucUP;
    halUAPSDInfo.srvInterval    = uServiceInt;
    halUAPSDInfo.susInterval    = uSuspendInt;
    halUAPSDInfo.delayInterval  = pTLCb->tlConfigInfo.uDelayedTriggerFrmInt;

    /*Notify HAL*/
    vosStatus = WDA_EnableUapsdAcParams(pvosGCtx, ucSTAId, &halUAPSDInfo);
  }
  return vosStatus;

}/*WLANTL_EnableUAPSDForAC*/


/*==========================================================================
  FUNCTION    WLANTL_DisableUAPSDForAC

  DESCRIPTION
   Called by HDD to disable UAPSD. TL in turn calls WDA API to disable the
   logic in FW/SLM to stop sending trigger frames. Previously TL had the
   trigger frame logic which later moved down to FW. Hence
   HDD -> TL -> WDA -> FW call flow.

  DEPENDENCIES
    The TL must be initialized before this function can be called.

  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    ucSTAId:        station Id
    ucAC:         AC for which U-APSD is being enabled


  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_SUCCESS:   Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_DisableUAPSDForAC
(
  v_PVOID_t          pvosGCtx,
  v_U8_t             ucSTAId,
  WLANTL_ACEnumType  ucAC
)
{
  WLANTL_CbType* pTLCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if (( NULL == pTLCb ) || WLANTL_STA_ID_INVALID( ucSTAId )
      ||   WLANTL_AC_INVALID(ucAC) )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid input params on WLANTL_DisableUAPSDForAC"
               " TL: %p  STA: %d  AC: %d", pTLCb, ucSTAId, ucAC ));
    return VOS_STATUS_E_FAULT;
  }

  if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  /*Reset this flag as this is no longer a trigger enabled AC*/
  pTLCb->atlSTAClients[ucSTAId]->wUAPSDInfo[ucAC].ucSet = 1;

  TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN TL:Disabling U-APSD in FW for STA: %d AC: %d ",
             ucSTAId, ucAC));

  /*Notify HAL*/
  WDA_DisableUapsdAcParams(pvosGCtx, ucSTAId, ucAC);

  return VOS_STATUS_SUCCESS;
}/* WLANTL_DisableUAPSDForAC */

#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
/*==========================================================================
  FUNCTION     WLANTL_RegRSSIIndicationCB

  DESCRIPTION  Registration function to get notification if RSSI cross
               threshold.
               Client should register threshold, direction, and notification
               callback function pointer

  DEPENDENCIES NONE
    
  PARAMETERS   in pAdapter - Global handle
               in rssiValue - RSSI threshold value
               in triggerEvent - Cross direction should be notified
                                 UP, DOWN, and CROSS
               in crossCBFunction - Notification CB Function
               in usrCtxt - user context

  RETURN VALUE VOS_STATUS

  SIDE EFFECTS NONE
  
============================================================================*/
VOS_STATUS WLANTL_RegRSSIIndicationCB
(
   v_PVOID_t                       pAdapter,
   v_S7_t                          rssiValue,
   v_U8_t                          triggerEvent,
   WLANTL_RSSICrossThresholdCBType crossCBFunction,
   VOS_MODULE_ID                   moduleID,
   v_PVOID_t                       usrCtxt
)
{
   VOS_STATUS                     status = VOS_STATUS_SUCCESS;

   status = WLANTL_HSRegRSSIIndicationCB(pAdapter,
                                         rssiValue,
                                         triggerEvent,
                                         crossCBFunction,
                                         moduleID,
                                         usrCtxt);

   return status;
}

/*==========================================================================
  FUNCTION     WLANTL_DeregRSSIIndicationCB

  DESCRIPTION  Remove specific threshold from list

  DEPENDENCIES NONE
    
  PARAMETERS   in pAdapter - Global handle
               in rssiValue - RSSI threshold value
               in triggerEvent - Cross direction should be notified
                                 UP, DOWN, and CROSS
   
  RETURN VALUE VOS_STATUS

  SIDE EFFECTS NONE
  
============================================================================*/
VOS_STATUS WLANTL_DeregRSSIIndicationCB
(
   v_PVOID_t                       pAdapter,
   v_S7_t                          rssiValue,
   v_U8_t                          triggerEvent,
   WLANTL_RSSICrossThresholdCBType crossCBFunction,
   VOS_MODULE_ID                   moduleID
)
{
   VOS_STATUS                     status = VOS_STATUS_SUCCESS;

   status = WLANTL_HSDeregRSSIIndicationCB(pAdapter,
                                           rssiValue,
                                           triggerEvent,
                                           crossCBFunction,
                                           moduleID);
   return status;
}

/*==========================================================================
  FUNCTION     WLANTL_SetAlpha

  DESCRIPTION  ALPLA is weight value to calculate AVG RSSI
               avgRSSI = (ALPHA * historyRSSI) + ((10 - ALPHA) * newRSSI)
               avgRSSI has (ALPHA * 10)% of history RSSI weight and
               (10 - ALPHA)% of newRSSI weight
               This portion is dynamically configurable.
               Default is ?

  DEPENDENCIES NONE
    
  PARAMETERS   in pAdapter - Global handle
               in valueAlpah - ALPHA
   
  RETURN VALUE VOS_STATUS

  SIDE EFFECTS NONE
  
============================================================================*/
VOS_STATUS WLANTL_SetAlpha
(
   v_PVOID_t pAdapter,
   v_U8_t    valueAlpha
)
{
   VOS_STATUS                     status = VOS_STATUS_SUCCESS;

   status = WLANTL_HSSetAlpha(pAdapter, valueAlpha);
   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_BMPSRSSIRegionChangedNotification
(
   v_PVOID_t             pAdapter,
   tpSirRSSINotification pRSSINotification
)
{
   VOS_STATUS                     status = VOS_STATUS_SUCCESS;

   status = WLANTL_HSBMPSRSSIRegionChangedNotification(pAdapter, pRSSINotification);
   return status;
}

/*==========================================================================
  FUNCTION     WLANTL_RegGetTrafficStatus

  DESCRIPTION  Registration function for traffic status monitoring
               During measure period count data frames.
               If frame count is larger then IDLE threshold set as traffic ON
               or OFF.
               And traffic status is changed send report to client with
               registered callback function

  DEPENDENCIES NONE
    
  PARAMETERS   in pAdapter - Global handle
               in idleThreshold - Traffic on or off threshold
               in measurePeriod - Traffic state check period
               in trfficStatusCB - traffic status changed notification
                                   CB function
               in usrCtxt - user context
   
  RETURN VALUE VOS_STATUS

  SIDE EFFECTS NONE
  
============================================================================*/
VOS_STATUS WLANTL_RegGetTrafficStatus
(
   v_PVOID_t                          pAdapter,
   v_U32_t                            idleThreshold,
   v_U32_t                            measurePeriod,
   WLANTL_TrafficStatusChangedCBType  trfficStatusCB,
   v_PVOID_t                          usrCtxt
)
{
   VOS_STATUS                     status = VOS_STATUS_SUCCESS;

   status = WLANTL_HSRegGetTrafficStatus(pAdapter,
                                idleThreshold,
                                measurePeriod,
                                trfficStatusCB,
                                usrCtxt);
   return status;
}
#endif
/*==========================================================================
  FUNCTION      WLANTL_GetStatistics

  DESCRIPTION   Get traffic statistics for identified station 

  DEPENDENCIES  NONE
    
  PARAMETERS    in pAdapter - Global handle
                in statType - specific statistics field to reset
                out statBuffer - traffic statistics buffer
   
  RETURN VALUE  VOS_STATUS

  SIDE EFFECTS  NONE
  
============================================================================*/
VOS_STATUS WLANTL_GetStatistics
(
   v_PVOID_t                  pAdapter,
   WLANTL_TRANSFER_STA_TYPE  *statBuffer,
   v_U8_t                     STAid
)
{
  WLANTL_CbType            *pTLCb  = VOS_GET_TL_CB(pAdapter);
  WLANTL_STAClientType*     pClientSTA = NULL;
  VOS_STATUS                status = VOS_STATUS_SUCCESS;
  WLANTL_TRANSFER_STA_TYPE *statistics = NULL;

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block 
   ------------------------------------------------------------------------*/
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Invalid TL pointer on WLANTL_GetStatistics"));
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[STAid];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  if(0 == pClientSTA->ucExists)
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL: %d STA ID does not exist", STAid));
    return VOS_STATUS_E_INVAL;
  }

  if(NULL == statBuffer)
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL:Invalid TL statistics buffer pointer on WLANTL_GetStatistics"));
    return VOS_STATUS_E_INVAL;
  }

  statistics = &pClientSTA->trafficStatistics;
  vos_mem_copy(statBuffer, statistics, sizeof(WLANTL_TRANSFER_STA_TYPE));

  return status;
}

/*==========================================================================
  FUNCTION      WLANTL_ResetStatistics

  DESCRIPTION   Reset statistics structure for identified station ID
                Reset means set values as 0

  DEPENDENCIES  NONE
    
  PARAMETERS    in pAdapter - Global handle
                in statType - specific statistics field to reset
   
  RETURN VALUE  VOS_STATUS

  SIDE EFFECTS  NONE
  
============================================================================*/
VOS_STATUS WLANTL_ResetStatistics
(
   v_PVOID_t                  pAdapter,
   v_U8_t                     STAid
)
{
  WLANTL_CbType            *pTLCb  = VOS_GET_TL_CB(pAdapter);
  WLANTL_STAClientType*     pClientSTA = NULL;
  VOS_STATUS                status = VOS_STATUS_SUCCESS;
  WLANTL_TRANSFER_STA_TYPE *statistics = NULL;

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block 
   ------------------------------------------------------------------------*/
  if (NULL == pTLCb) 
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Invalid TL pointer on WLANTL_GetStatistics"));
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[STAid];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  if(0 == pClientSTA->ucExists)
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL: %d STA ID does not exist", STAid));
    return VOS_STATUS_E_INVAL;
  }

  statistics = &pClientSTA->trafficStatistics;
  vos_mem_zero((v_VOID_t *)statistics, sizeof(WLANTL_TRANSFER_STA_TYPE));

  return status;
}

/*==========================================================================
  FUNCTION      WLANTL_GetSpecStatistic

  DESCRIPTION   Get specific field within statistics structure for
                identified station ID 

  DEPENDENCIES  NONE

  PARAMETERS    in pAdapter - Global handle
                in statType - specific statistics field to reset
                in STAid    - Station ID
                out buffer  - Statistic value
   
  RETURN VALUE  VOS_STATUS

  SIDE EFFECTS  NONE
  
============================================================================*/
VOS_STATUS WLANTL_GetSpecStatistic
(
   v_PVOID_t                    pAdapter,
   WLANTL_TRANSFER_STATIC_TYPE  statType,
   v_U32_t                     *buffer,
   v_U8_t                       STAid
)
{
  WLANTL_CbType            *pTLCb  = VOS_GET_TL_CB(pAdapter);
  WLANTL_STAClientType*     pClientSTA = NULL;
  VOS_STATUS                status = VOS_STATUS_SUCCESS;
  WLANTL_TRANSFER_STA_TYPE *statistics = NULL;

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block 
   ------------------------------------------------------------------------*/
  if (NULL == pTLCb) 
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Invalid TL pointer on WLANTL_GetStatistics"));
    return VOS_STATUS_E_FAULT;
  }
  pClientSTA = pTLCb->atlSTAClients[STAid];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  if(0 == pClientSTA->ucExists)
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL: %d STA ID does not exist", STAid));
    return VOS_STATUS_E_INVAL;
  }

  if(NULL == buffer)
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL:Invalid TL statistic buffer pointer on WLANTL_GetStatistics"));
    return VOS_STATUS_E_INVAL;
  }

  statistics = &pClientSTA->trafficStatistics;
  switch(statType)
  {
    case WLANTL_STATIC_TX_UC_FCNT:
      *buffer = statistics->txUCFcnt;
      break;

    case WLANTL_STATIC_TX_MC_FCNT:
      *buffer = statistics->txMCFcnt;
      break;

    case WLANTL_STATIC_TX_BC_FCNT:
      *buffer = statistics->txBCFcnt;
      break;

    case WLANTL_STATIC_TX_UC_BCNT:
      *buffer = statistics->txUCBcnt;
      break;

    case WLANTL_STATIC_TX_MC_BCNT:
      *buffer = statistics->txMCBcnt;
      break;

    case WLANTL_STATIC_TX_BC_BCNT:
      *buffer = statistics->txBCBcnt;
      break;

    case WLANTL_STATIC_RX_UC_FCNT:
      *buffer = statistics->rxUCFcnt;
      break;

    case WLANTL_STATIC_RX_MC_FCNT:
      *buffer = statistics->rxMCFcnt;
      break;

    case WLANTL_STATIC_RX_BC_FCNT:
      *buffer = statistics->rxBCFcnt;
      break;

    case WLANTL_STATIC_RX_UC_BCNT:
      *buffer = statistics->rxUCBcnt;
      break;

    case WLANTL_STATIC_RX_MC_BCNT:
      *buffer = statistics->rxMCBcnt;
      break;

    case WLANTL_STATIC_RX_BC_BCNT:
      *buffer = statistics->rxBCBcnt;
      break;

    case WLANTL_STATIC_RX_BCNT:
      *buffer = statistics->rxBcnt;
      break;

    case WLANTL_STATIC_RX_BCNT_CRC_OK:
      *buffer = statistics->rxBcntCRCok;
      break;

    case WLANTL_STATIC_RX_RATE:
      *buffer = statistics->rxRate;
      break;

    default:
      *buffer = 0;
      status = VOS_STATUS_E_INVAL;
      break;
  }


  return status;
}

/*==========================================================================
  FUNCTION      WLANTL_ResetSpecStatistic

  DESCRIPTION   Reset specific field within statistics structure for
                identified station ID
                Reset means set as 0

  DEPENDENCIES  NONE
    
  PARAMETERS    in pAdapter - Global handle
                in statType - specific statistics field to reset
                in STAid    - Station ID

  RETURN VALUE  VOS_STATUS

  SIDE EFFECTS  NONE
  
============================================================================*/
VOS_STATUS WLANTL_ResetSpecStatistic
(
   v_PVOID_t                    pAdapter,
   WLANTL_TRANSFER_STATIC_TYPE  statType,
   v_U8_t                       STAid
)
{
  WLANTL_CbType            *pTLCb  = VOS_GET_TL_CB(pAdapter);
  WLANTL_STAClientType*     pClientSTA = NULL;
  VOS_STATUS                status = VOS_STATUS_SUCCESS;
  WLANTL_TRANSFER_STA_TYPE *statistics = NULL;

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block 
   ------------------------------------------------------------------------*/
  if (NULL == pTLCb) 
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "WLAN TL:Invalid TL pointer on WLANTL_GetStatistics"));
    return VOS_STATUS_E_FAULT;
  }

  pClientSTA = pTLCb->atlSTAClients[STAid];

  if ( NULL == pClientSTA )
  {
      TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
          "WLAN TL:Client Memory was not allocated on %s", __func__));
      return VOS_STATUS_E_FAILURE;
  }

  if(0 == pClientSTA->ucExists)
  {
    TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
    "WLAN TL: %d STA ID does not exist", STAid));
    return VOS_STATUS_E_INVAL;
  }

  statistics = &pClientSTA->trafficStatistics;
  switch(statType)
  {
    case WLANTL_STATIC_TX_UC_FCNT:
      statistics->txUCFcnt = 0;
      break;

    case WLANTL_STATIC_TX_MC_FCNT:
      statistics->txMCFcnt = 0;
      break;

    case WLANTL_STATIC_TX_BC_FCNT:
      statistics->txBCFcnt = 0;
      break;

    case WLANTL_STATIC_TX_UC_BCNT:
      statistics->txUCBcnt = 0;
      break;

    case WLANTL_STATIC_TX_MC_BCNT:
      statistics->txMCBcnt = 0;
      break;

    case WLANTL_STATIC_TX_BC_BCNT:
      statistics->txBCBcnt = 0;
      break;

    case WLANTL_STATIC_RX_UC_FCNT:
      statistics->rxUCFcnt = 0;
      break;

    case WLANTL_STATIC_RX_MC_FCNT:
      statistics->rxMCFcnt = 0;
      break;

    case WLANTL_STATIC_RX_BC_FCNT:
      statistics->rxBCFcnt = 0;
      break;

    case WLANTL_STATIC_RX_UC_BCNT:
      statistics->rxUCBcnt = 0;
      break;

    case WLANTL_STATIC_RX_MC_BCNT:
      statistics->rxMCBcnt = 0;
      break;

    case WLANTL_STATIC_RX_BC_BCNT:
      statistics->rxBCBcnt = 0;
      break;

    case WLANTL_STATIC_RX_BCNT:
      statistics->rxBcnt = 0;
      break;

    case WLANTL_STATIC_RX_BCNT_CRC_OK:
      statistics->rxBcntCRCok = 0;
      break;

    case WLANTL_STATIC_RX_RATE:
      statistics->rxRate = 0;
      break;

    default:
      status = VOS_STATUS_E_INVAL;
      break;
  }

  return status;
}


/*==========================================================================

   FUNCTION

   DESCRIPTION   Read RSSI value out of a RX BD
    
   PARAMETERS:  Caller must validate all parameters 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_ReadRSSI
(
   v_PVOID_t        pAdapter,
   v_PVOID_t        pBDHeader,
   v_U8_t           STAid
)
{
   WLANTL_CbType   *tlCtxt = VOS_GET_TL_CB(pAdapter);
   v_S7_t           currentRSSI, currentRSSI0, currentRSSI1;


   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR, "%s Invalid TL handle", __func__));
      return VOS_STATUS_E_INVAL;
   }

   if ( NULL == tlCtxt->atlSTAClients[STAid] )
   {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Client Memory was not allocated on %s", __func__));
       return VOS_STATUS_E_FAILURE;
   }

   currentRSSI0 = WLANTL_GETRSSI0(pBDHeader);
   currentRSSI1 = WLANTL_GETRSSI1(pBDHeader);
   currentRSSI  = (currentRSSI0 > currentRSSI1) ? currentRSSI0 : currentRSSI1;

   tlCtxt->atlSTAClients[STAid]->rssiAvg = currentRSSI;

   return VOS_STATUS_SUCCESS;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION   Read SNR value out of a RX BD

   PARAMETERS:  Caller must validate all parameters

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_ReadSNR
(
   v_PVOID_t        pAdapter,
   v_PVOID_t        pBDHeader,
   v_U8_t           STAid
)
{
   WLANTL_CbType   *tlCtxt = VOS_GET_TL_CB(pAdapter);
   v_S7_t           currentSNR;


   if (NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                       "%s Invalid TL handle", __func__));
      return VOS_STATUS_E_INVAL;
   }

   if (NULL == tlCtxt->atlSTAClients[STAid])
   {
       TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
           "WLAN TL:Client Memory was not allocated on %s", __func__));
       return VOS_STATUS_E_FAILURE;
   }

   currentSNR = WLANTL_GETSNR(pBDHeader);

   /* SNR reported in the Buffer Descriptor is scaled up by 2(SNR*2),
    * Get the correct SNR value
    */
   currentSNR = currentSNR >> 1;

   /* SNR reported by HW cannot be more than 35dB due to HW limitations */
   currentSNR = (WLANTL_MAX_HW_SNR > currentSNR ? currentSNR :
                                                  WLANTL_MAX_HW_SNR);

   TLLOG2(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
          "%s: snrsum: %d snridx: %d prevsnravg: %d",
           __func__,
           tlCtxt->atlSTAClients[STAid]->snrSum,
           tlCtxt->atlSTAClients[STAid]->snrIdx,
           tlCtxt->atlSTAClients[STAid]->prevSnrAvg));

   /* The SNR returned for all purposes is the average SNR over
    * WLANTL_MAX_SNR_DATA_SMAPLES.When data samples
    * > WLANTL_MAX_SNR_DATA_SAMPLES are obtained,
    * store the average of the samples in prevSnrAvg
    * and start a new averaging window. The prevSnrAvg is used when
    * enough data samples are not available when applications
    * actually query for SNR.
    *
    * SEE: WLANTL_GetSnr()
    */
   if (tlCtxt->atlSTAClients[STAid]->snrIdx >= WLANTL_MAX_SNR_DATA_SAMPLES)
   {
       tlCtxt->atlSTAClients[STAid]->prevSnrAvg =
               tlCtxt->atlSTAClients[STAid]->snrSum /
               tlCtxt->atlSTAClients[STAid]->snrIdx;
       tlCtxt->atlSTAClients[STAid]->snrSum = 0;
       tlCtxt->atlSTAClients[STAid]->snrIdx = 0;
   }
   tlCtxt->atlSTAClients[STAid]->snrSum += currentSNR;
   tlCtxt->atlSTAClients[STAid]->snrIdx += 1;

   return VOS_STATUS_SUCCESS;
}

/*
 DESCRIPTION 
    TL returns the weight currently maintained in TL.
 IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 

 OUT
    pACWeights:     Caller allocated memory for filling in weights

 RETURN VALUE  VOS_STATUS
*/
VOS_STATUS  
WLANTL_GetACWeights 
( 
  v_PVOID_t             pvosGCtx,
  v_U8_t*               pACWeights
)
{
   WLANTL_CbType*  pTLCb = NULL;
   v_U8_t          ucIndex; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == pACWeights )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_GetACWeights"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_GetACWeights"));
    return VOS_STATUS_E_FAULT;
  }
  for ( ucIndex = 0; ucIndex < WLANTL_MAX_AC ; ucIndex++)
  {
    pACWeights[ucIndex] = pTLCb->tlConfigInfo.ucAcWeights[ucIndex];
  }

  return VOS_STATUS_SUCCESS;
}



/*
 DESCRIPTION 
    Change the weight currently maintained by TL.
 IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 
    pACWeights:     Caller allocated memory contain the weights to use


 RETURN VALUE  VOS_STATUS
*/
VOS_STATUS  
WLANTL_SetACWeights 
( 
  v_PVOID_t             pvosGCtx,
  v_U8_t*               pACWeights
)
{
   WLANTL_CbType*  pTLCb = NULL;
   v_U8_t          ucIndex; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == pACWeights )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Invalid parameter sent on WLANTL_GetACWeights"));
    return VOS_STATUS_E_INVAL;
  }

  /*------------------------------------------------------------------------
    Extract TL control block and check existance
   ------------------------------------------------------------------------*/
  pTLCb = VOS_GET_TL_CB(pvosGCtx);
  if ( NULL == pTLCb )
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
              "WLAN TL:Invalid TL pointer from pvosGCtx on WLANTL_GetACWeights"));
    return VOS_STATUS_E_FAULT;
  }
  for ( ucIndex = 0; ucIndex < WLANTL_MAX_AC ; ucIndex++)
  {
    pTLCb->tlConfigInfo.ucAcWeights[ucIndex] = pACWeights[ucIndex];
  }

  pTLCb->tlConfigInfo.ucAcWeights[WLANTL_AC_HIGH_PRIO] = pACWeights[WLANTL_AC_VO];
  return VOS_STATUS_SUCCESS;
}


/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
void WLANTL_PowerStateChangedCB
(
   v_PVOID_t pAdapter,
   tPmcState newState
)
{
   WLANTL_CbType                *tlCtxt = VOS_GET_TL_CB(pAdapter);

   if (NULL == tlCtxt)
   {
     VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "%s: Invalid TL Control Block", __func__ );
     return;
   }

   VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO, "Power state changed, new state is %d", newState );
   switch(newState)
   {
      case FULL_POWER:
         tlCtxt->isBMPS = VOS_FALSE;
         break;

      case BMPS:
#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
         WLANTL_SetFWRSSIThresholds(pAdapter);
#endif

         tlCtxt->isBMPS = VOS_TRUE;
         break;

      case IMPS:
      case LOW_POWER:
      case REQUEST_BMPS:
      case REQUEST_FULL_POWER:
      case REQUEST_IMPS:
      case STOPPED:
      case REQUEST_START_UAPSD:
      case REQUEST_STOP_UAPSD:
      case UAPSD:
      case REQUEST_STANDBY:
      case STANDBY:
      case REQUEST_ENTER_WOWL:
      case REQUEST_EXIT_WOWL:
      case WOWL:
         TLLOGW(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN, "Not handle this events %d", newState ));
         break;

      default:
         TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR, "Not a valid event %d", newState ));
         break;
   }

   return;
}
/*==========================================================================
  FUNCTION      WLANTL_GetEtherType

  DESCRIPTION   Extract Ether type information from the BD

  DEPENDENCIES  NONE
    
  PARAMETERS    in aucBDHeader - BD header
                in vosDataBuff - data buffer
                in ucMPDUHLen  - MPDU header length
                out pUsEtherType - pointer to Ethertype

  RETURN VALUE  VOS_STATUS_SUCCESS : if the EtherType is successfully extracted
                VOS_STATUS_FAILURE : if the EtherType extraction failed and
                                     the packet was dropped

  SIDE EFFECTS  NONE
  
============================================================================*/
static VOS_STATUS WLANTL_GetEtherType
(
   v_U8_t               * aucBDHeader,
   vos_pkt_t            * vosDataBuff,
   v_U8_t                 ucMPDUHLen,
   v_U16_t              * pUsEtherType
)
{
  v_U8_t                   ucOffset;
  v_U16_t                  usEtherType = *pUsEtherType;
  v_SIZE_t                 usLLCSize = sizeof(usEtherType);
  VOS_STATUS               vosStatus  = VOS_STATUS_SUCCESS;
  
  /*------------------------------------------------------------------------
    Check if LLC is present - if not, TL is unable to determine type
   ------------------------------------------------------------------------*/
  if ( VOS_FALSE == WDA_IS_RX_LLC_PRESENT( aucBDHeader ) )
  {
    ucOffset = WLANTL_802_3_HEADER_LEN - sizeof(usEtherType); 
  }
  else
  {
    ucOffset = ucMPDUHLen + WLANTL_LLC_PROTO_TYPE_OFFSET;
  }

  /*------------------------------------------------------------------------
    Extract LLC type 
  ------------------------------------------------------------------------*/
  vosStatus = vos_pkt_extract_data( vosDataBuff, ucOffset, 
                                    (v_PVOID_t)&usEtherType, &usLLCSize); 

  if (( VOS_STATUS_SUCCESS != vosStatus ) || 
      ( sizeof(usEtherType) != usLLCSize ))
      
  {
    TLLOGE(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:Error extracting Ether type from data packet"));
    /* Drop packet */
    vos_pkt_return_packet(vosDataBuff);
    vosStatus = VOS_STATUS_E_FAILURE;
  }
  else
  {
    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:Ether type retrieved before endianess conv: %d", 
               usEtherType));

    usEtherType = vos_be16_to_cpu(usEtherType);
    *pUsEtherType = usEtherType;

    TLLOG2(VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
               "WLAN TL:Ether type retrieved: %d", usEtherType));
  }
  
  return vosStatus;
}

/*==========================================================================
  FUNCTION      WLANTL_GetSoftAPStatistics

  DESCRIPTION   Collect the cumulative statistics for all Softap stations

  DEPENDENCIES  NONE
    
  PARAMETERS    in pvosGCtx  - Pointer to the global vos context
                   bReset    - If set TL statistics will be cleared after reading
                out statsSum - pointer to collected statistics

  RETURN VALUE  VOS_STATUS_SUCCESS : if the Statistics are successfully extracted

  SIDE EFFECTS  NONE

============================================================================*/
VOS_STATUS WLANTL_GetSoftAPStatistics(v_PVOID_t pAdapter, WLANTL_TRANSFER_STA_TYPE *statsSum, v_BOOL_t bReset)
{
    v_U8_t i = 0;
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    WLANTL_CbType *pTLCb  = VOS_GET_TL_CB(pAdapter);
    WLANTL_TRANSFER_STA_TYPE statBufferTemp;
    vos_mem_zero((v_VOID_t *)&statBufferTemp, sizeof(WLANTL_TRANSFER_STA_TYPE));
    vos_mem_zero((v_VOID_t *)statsSum, sizeof(WLANTL_TRANSFER_STA_TYPE));


    if ( NULL == pTLCb )
    {
       return VOS_STATUS_E_FAULT;
    } 

    // Sum up all the statistics for stations of Soft AP from TL
    for (i = 0; i < WLAN_MAX_STA_COUNT; i++)
    {
        if ( NULL == pTLCb->atlSTAClients[i])
        {
            continue;
        }
        if (pTLCb->atlSTAClients[i]->wSTADesc.wSTAType == WLAN_STA_SOFTAP)
        {
           vosStatus = WLANTL_GetStatistics(pAdapter, &statBufferTemp, i);// Can include staId 1 because statistics not collected for it

           if (!VOS_IS_STATUS_SUCCESS(vosStatus))
                return VOS_STATUS_E_FAULT;

            // Add to the counters
           statsSum->txUCFcnt += statBufferTemp.txUCFcnt;
           statsSum->txMCFcnt += statBufferTemp.txMCFcnt;
           statsSum->txBCFcnt += statBufferTemp.txBCFcnt;
           statsSum->txUCBcnt += statBufferTemp.txUCBcnt;
           statsSum->txMCBcnt += statBufferTemp.txMCBcnt;
           statsSum->txBCBcnt += statBufferTemp.txBCBcnt;
           statsSum->rxUCFcnt += statBufferTemp.rxUCFcnt;
           statsSum->rxMCFcnt += statBufferTemp.rxMCFcnt;
           statsSum->rxBCFcnt += statBufferTemp.rxBCFcnt;
           statsSum->rxUCBcnt += statBufferTemp.rxUCBcnt;
           statsSum->rxMCBcnt += statBufferTemp.rxMCBcnt;
           statsSum->rxBCBcnt += statBufferTemp.rxBCBcnt;

           if (bReset)
           {
              vosStatus = WLANTL_ResetStatistics(pAdapter, i);
              if (!VOS_IS_STATUS_SUCCESS(vosStatus))
                return VOS_STATUS_E_FAULT;               
          }
        }
    }

    return vosStatus;
}

/*===============================================================================
  FUNCTION      WLANTL_IsReplayPacket
     
  DESCRIPTION   This function does replay check for valid stations
 
  DEPENDENCIES  Validity of replay check must be done before the function 
                is called
                          
  PARAMETERS    currentReplayCounter    current replay counter taken from RX BD 
                previousReplayCounter   previous replay counter taken from TL CB
                                       
  RETRUN        VOS_TRUE    packet is a replay packet
                VOS_FALSE   packet is not a replay packet

  SIDE EFFECTS   none
 ===============================================================================*/
v_BOOL_t
WLANTL_IsReplayPacket
(
  v_U64_t    ullcurrentReplayCounter,
  v_U64_t    ullpreviousReplayCounter
)
{
   /* Do the replay check by comparing previous received replay counter with
      current received replay counter*/
    if(ullpreviousReplayCounter < ullcurrentReplayCounter)
    {
        /* Valid packet not replay */
        return VOS_FALSE;
    }
    else
    {

        /* Current packet number is less than or equal to previuos received 
           packet no, this means current packet is replay packet */
        VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL: Replay packet found with replay counter :[0x%llX]",ullcurrentReplayCounter);
           
        return VOS_TRUE;
    }
}

#if 0
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
WLANTL_GetReplayCounterFromRxBD
(
   v_U8_t *pucRxBDHeader
)
{
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
    v_U64_t ullcurrentReplayCounter = 0;
    /* Getting 48-bit replay counter from the RX BD */
    ullcurrentReplayCounter = WLANHAL_RX_BD_GET_PMICMD_20TO23(pucRxBDHeader); 
    ullcurrentReplayCounter <<= 16;
    ullcurrentReplayCounter |= (( WLANHAL_RX_BD_GET_PMICMD_24TO25(pucRxBDHeader) & 0xFFFF0000) >> 16);
    return ullcurrentReplayCounter;
#else
    v_U64_t ullcurrentReplayCounter = 0;
    /* Getting 48-bit replay counter from the RX BD */
    ullcurrentReplayCounter = (WLANHAL_RX_BD_GET_PMICMD_24TO25(pucRxBDHeader) & 0x0000FFFF); 
    ullcurrentReplayCounter <<= 32; 
    ullcurrentReplayCounter |= WLANHAL_RX_BD_GET_PMICMD_20TO23(pucRxBDHeader); 
    return ullcurrentReplayCounter;
#endif
}
#endif

/*===============================================================================
  FUNCTION      WLANTL_PostResNeeded
     
  DESCRIPTION   This function posts message to TL to reserve BD/PDU memory
 
  DEPENDENCIES  None
                          
  PARAMETERS    pvosGCtx
                                       
  RETURN        None

  SIDE EFFECTS   none
 ===============================================================================*/

void WLANTL_PostResNeeded(v_PVOID_t pvosGCtx)
{
  vos_msg_t            vosMsg;

  vosMsg.reserved = 0;
  vosMsg.bodyptr  = NULL;
  vosMsg.type     = WLANTL_TX_RES_NEEDED;
  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
        "WLAN TL: BD/PDU available interrupt received, Posting message to TL");
  if(!VOS_IS_STATUS_SUCCESS(vos_tx_mq_serialize( VOS_MQ_ID_TL, &vosMsg)))
  {
    VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
       " %s fails to post message", __func__);
  }
}

/*===============================================================================
  FUNCTION       WLANTL_UpdateRssiBmps

  DESCRIPTION    This function updates the TL's RSSI (in BMPS mode)

  DEPENDENCIES   None

  PARAMETERS

    pvosGCtx         VOS context          VOS Global context
    staId            Station ID           Station ID
    rssi             RSSI (BMPS mode)     RSSI in BMPS mode

  RETURN         None

  SIDE EFFECTS   none
 ===============================================================================*/

void WLANTL_UpdateRssiBmps(v_PVOID_t pvosGCtx, v_U8_t staId, v_S7_t rssi)
{
  WLANTL_CbType* pTLCb = VOS_GET_TL_CB(pvosGCtx);

  if (NULL != pTLCb && NULL != pTLCb->atlSTAClients[staId])
  {
    pTLCb->atlSTAClients[staId]->rssiAvgBmps = rssi;
  }
}

/*===============================================================================
  FUNCTION       WLANTL_UpdateSnrBmps

  DESCRIPTION    This function updates the TL's SNR (in BMPS mode)

  DEPENDENCIES   None

  PARAMETERS

    pvosGCtx         VOS context          VOS Global context
    staId            Station ID           Station ID
    snr             SNR (BMPS mode)     SNR in BMPS mode

  RETURN         None

  SIDE EFFECTS   none
 ===============================================================================*/

void WLANTL_UpdateSnrBmps(v_PVOID_t pvosGCtx, v_U8_t staId, v_S7_t snr)
{
  WLANTL_CbType* pTLCb = VOS_GET_TL_CB(pvosGCtx);

  if (NULL != pTLCb && NULL != pTLCb->atlSTAClients[staId])
  {
    pTLCb->atlSTAClients[staId]->snrAvgBmps = snr;
  }
}

/*===============================================================================
  FUNCTION       WLANTL_UpdateLinkCapacity

  DESCRIPTION    This function updates the STA's Link Capacity in TL

  DEPENDENCIES   None

  PARAMETERS

    pvosGCtx         VOS context          VOS Global context
    staId            Station ID           Station ID
    linkCapacity     linkCapacity         Link Capacity

  RETURN         None

  SIDE EFFECTS   none
 ===============================================================================*/

void WLANTL_UpdateLinkCapacity(v_PVOID_t pvosGCtx, v_U8_t staId, v_U32_t linkCapacity)
{
    WLANTL_CbType* pTLCb = VOS_GET_TL_CB(pvosGCtx);

    if (NULL != pTLCb && NULL != pTLCb->atlSTAClients[staId])
    {
        pTLCb->atlSTAClients[staId]->linkCapacity = linkCapacity;
    }
}


/*===========================================================================

  FUNCTION    WLANTL_GetSTALinkCapacity

  DESCRIPTION

    Returns Link Capacity of a particular STA.

  DEPENDENCIES

    A station must have been registered before its state can be retrieved.


  PARAMETERS

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's
                    control block can be extracted from its context
    ucSTAId:        identifier of the station

    OUT
    plinkCapacity:  the current link capacity the connection to
                    the given station


  RETURN VALUE

    The result code associated with performing the operation

    VOS_STATUS_E_INVAL:  Input parameters are invalid
    VOS_STATUS_E_FAULT:  Station ID is outside array boundaries or pointer to
                         TL cb is NULL ; access would cause a page fault
    VOS_STATUS_E_EXISTS: Station was not registered
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WLANTL_GetSTALinkCapacity
(
    v_PVOID_t             pvosGCtx,
    v_U8_t                ucSTAId,
    v_U32_t               *plinkCapacity
)
{
    WLANTL_CbType*  pTLCb = NULL;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /*------------------------------------------------------------------------
      Sanity check
     ------------------------------------------------------------------------*/
    if ( NULL == plinkCapacity )
    {
        TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                         FL("WLAN TL:Invalid parameter")));
        return VOS_STATUS_E_INVAL;
    }

    if ( WLANTL_STA_ID_INVALID( ucSTAId ) )
    {
        TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                         FL("WLAN TL:Invalid station id")));
        return VOS_STATUS_E_FAULT;
    }

    /*------------------------------------------------------------------------
      Extract TL control block and check existance
     ------------------------------------------------------------------------*/
    pTLCb = VOS_GET_TL_CB(pvosGCtx);
    if ( NULL == pTLCb )
    {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                          FL("WLAN TL:Invalid TL pointer from pvosGCtx")));
         return VOS_STATUS_E_FAULT;
    }

    if ( NULL == pTLCb->atlSTAClients[ucSTAId] )
    {
        TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                         FL("WLAN TL:Client Memory was not allocated")));
        return VOS_STATUS_E_FAILURE;
    }

    if ( 0 == pTLCb->atlSTAClients[ucSTAId]->ucExists )
    {
        TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                         FL("WLAN TL:Station was not previously registered")));
        return VOS_STATUS_E_EXISTS;
    }

    /*------------------------------------------------------------------------
      Get STA state
     ------------------------------------------------------------------------*/
    *plinkCapacity = pTLCb->atlSTAClients[ucSTAId]->linkCapacity;

    return VOS_STATUS_SUCCESS;
}/* WLANTL_GetSTALinkCapacity */
