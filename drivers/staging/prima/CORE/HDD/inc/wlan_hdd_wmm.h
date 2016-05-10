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
#ifndef _WLAN_HDD_WMM_H
#define _WLAN_HDD_WMM_H
/*============================================================================
  @file wlan_hdd_wmm.h

  This module (wlan_hdd_wmm.h interface + wlan_hdd_wmm.c implementation)
  houses all the logic for WMM in HDD.

  On the control path, it has the logic to setup QoS, modify QoS and delete
  QoS (QoS here refers to a TSPEC). The setup QoS comes in two flavors: an
  explicit application invoked and an internal HDD invoked.  The implicit QoS
  is for applications that do NOT call the custom QCT WLAN OIDs for QoS but
  which DO mark their traffic for priortization. It also has logic to start,
  update and stop the U-APSD trigger frame generation. It also has logic to
  read WMM related config parameters from the registry.

  On the data path, it has the logic to figure out the WMM AC of an egress
  packet and when to signal TL to serve a particular AC queue. It also has the
  logic to retrieve a packet based on WMM priority in response to a fetch from
  TL.

  The remaining functions are utility functions for information hiding.


               Copyright (c) 2008-9 Qualcomm Technologies, Inc.
               All Rights Reserved.
               Qualcomm Technologies Confidential and Proprietary
============================================================================*/
/* $Header$ */

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include <linux/workqueue.h>
#include <linux/list.h>
#include <wlan_hdd_main.h>
#include <wlan_hdd_wext.h>
#include <wlan_qct_tl.h>
#include <sme_QosApi.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

// #define HDD_WMM_DEBUG 1

#define HDD_WMM_CTX_MAGIC 0x574d4d58    // "WMMX"

#define HDD_WMM_HANDLE_IMPLICIT 0xFFFFFFFF

#define HDD_WLAN_INVALID_STA_ID 0xFF

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
/*! @brief AC/Queue Index values for Linux Qdisc to operate on different traffic.
*/
typedef enum
{
   HDD_LINUX_AC_VO = 0,
   HDD_LINUX_AC_VI = 1,
   HDD_LINUX_AC_BE = 2,
   HDD_LINUX_AC_BK = 3

} hdd_wmm_linuxac_t;
 
/*! @brief types of classification supported
*/
typedef enum
{
   HDD_WMM_CLASSIFICATION_DSCP = 0,
   HDD_WMM_CLASSIFICATION_802_1Q = 1

} hdd_wmm_classification_t;

/*! @brief UAPSD state
*/
typedef enum
{
   HDD_WMM_NON_UAPSD = 0,
   HDD_WMM_UAPSD = 1

} hdd_wmm_uapsd_state_t;


typedef enum
{
   //STA can associate with any AP, & HDD looks at the SME notification after
   // association to find out if associated with QAP and acts accordingly
   HDD_WMM_USER_MODE_AUTO = 0,
   //SME will add the extra logic to make sure STA associates with a QAP only
   HDD_WMM_USER_MODE_QBSS_ONLY = 1,
   //SME will not join a QoS AP, unless the phy mode setting says "Auto". In
   // that case, STA is free to join 11n AP. Although from HDD point of view,
   // it will not be doing any packet classifications
   HDD_WMM_USER_MODE_NO_QOS = 2,

} hdd_wmm_user_mode_t;

// UAPSD Mask bits
// (Bit0:VO; Bit1:VI; Bit2:BK; Bit3:BE all other bits are ignored)
#define HDD_AC_VO 0x1
#define HDD_AC_VI 0x2
#define HDD_AC_BK 0x4
#define HDD_AC_BE 0x8

/*! @brief WMM Qos instance control block
*/
typedef struct
{
   struct list_head             node;
   v_U32_t                      handle;
   v_U32_t                      qosFlowId;
   hdd_adapter_t*               pAdapter;
   WLANTL_ACEnumType            acType;
   hdd_wlan_wmm_status_e        lastStatus;
   struct work_struct           wmmAcSetupImplicitQos;
   v_U32_t                      magic;
} hdd_wmm_qos_context_t;

/*! @brief WMM related per-AC state & status info
*/
typedef struct
{
   // does the AP require access to this AC?
   v_BOOL_t                     wmmAcAccessRequired;

   // does the worker thread need to acquire access to this AC?
   v_BOOL_t                     wmmAcAccessNeeded;

   // is implicit QoS negotiation currently taking place?
   v_BOOL_t                     wmmAcAccessPending;

   // has implicit QoS negotiation already failed?
   v_BOOL_t                     wmmAcAccessFailed;

   // has implicit QoS negotiation already succeeded?
   v_BOOL_t                     wmmAcAccessGranted;

   // is access to this AC allowed, either because we are not doing
   // WMM, we are not doing implicit QoS, implict QoS has completed,
   // or explicit QoS has completed?
   v_BOOL_t                     wmmAcAccessAllowed;

   // is the wmmAcTspecInfo valid?
   v_BOOL_t                     wmmAcTspecValid;

   // are the wmmAcUapsd* fields valid?
   v_BOOL_t                     wmmAcUapsdInfoValid;

   // current (possibly aggregate) Tspec for this AC
   sme_QosWmmTspecInfo          wmmAcTspecInfo;

   // current U-APSD parameters
   v_BOOL_t                     wmmAcIsUapsdEnabled;
   v_U32_t                      wmmAcUapsdServiceInterval;
   v_U32_t                      wmmAcUapsdSuspensionInterval;
   sme_QosWmmDirType            wmmAcUapsdDirection;

#ifdef FEATURE_WLAN_CCX
   // Inactivity time parameters for TSPEC
   v_U32_t                      wmmInactivityTime;
   v_U32_t                      wmmPrevTrafficCnt;
   vos_timer_t                  wmmInactivityTimer;
#endif

} hdd_wmm_ac_status_t;

/*! @brief WMM state & status info
*/
typedef struct
{
   struct list_head             wmmContextList;
   struct mutex                 wmmLock;
   hdd_wmm_ac_status_t          wmmAcStatus[WLANTL_MAX_AC];
   v_BOOL_t                     wmmQap;
   v_BOOL_t                     wmmQosConnection;
} hdd_wmm_status_t;

extern const v_U8_t hdd_QdiscAcToTlAC[];
extern const v_U8_t hddWmmUpToAcMap[]; 
extern const v_U8_t hddLinuxUpToAcMap[];

/**============================================================================
  @brief hdd_wmm_init() - Function which will initialize the WMM configuation
  and status to an initial state.  The configuration can later be overwritten
  via application APIs

  @param pHddCtx : [in]  pointer to HDD context

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure

  ===========================================================================*/
VOS_STATUS hdd_wmm_init ( hdd_context_t* pHddCtx );

/**============================================================================
  @brief hdd_wmm_adapter_init() - Function which will initialize the WMM configuation
  and status to an initial state.  The configuration can later be overwritten
  via application APIs

  @param pAdapter : [in]  pointer to Adapter context

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure

  ===========================================================================*/
VOS_STATUS hdd_wmm_adapter_init( hdd_adapter_t *pAdapter );

/**============================================================================
  @brief hdd_wmm_adapter_close() - Function which will perform any necessary work to
  to clean up the WMM functionality prior to the kernel module unload

  @param pAdapter : [in]  pointer to adapter context

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure

  ===========================================================================*/
VOS_STATUS hdd_wmm_adapter_close ( hdd_adapter_t* pAdapter );

/**============================================================================
  @brief hdd_wmm_select_queue() - Function which will classify an OS packet
  into linux Qdisc expectation

  @param dev      : [in]  pointer to net_device structure
  @param skb      : [in]  pointer to OS packet (sk_buff)

  @return         : queue_index/linux AC value.
  ===========================================================================*/
v_U16_t hdd_wmm_select_queue(struct net_device * dev, struct sk_buff *skb);

/**============================================================================
  @brief hdd_hostapd_select_queue() - Function which will classify the packet
         according to linux qdisc expectation.


  @param dev      : [in]  pointer to net_device structure
  @param skb      : [in]  pointer to os packet

  @return         : Qdisc queue index
  ===========================================================================*/

v_U16_t hdd_hostapd_select_queue(struct net_device * dev, struct sk_buff *skb);



/**============================================================================
  @brief hdd_wmm_classify_pkt() - Function which will classify an OS packet
  into a WMM AC based on either 802.1Q or DSCP

  @param pAdapter : [in]  pointer to adapter context
  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param pAcType  : [out] pointer to WMM AC type of OS packet

  @return         : None
  ===========================================================================*/
v_VOID_t hdd_wmm_classify_pkt ( hdd_adapter_t* pAdapter,
                                struct sk_buff *skb,
                                WLANTL_ACEnumType* pAcType,
                                sme_QosWmmUpType* pUserPri);


/**============================================================================
  @brief hdd_wmm_acquire_access() - Function which will attempt to acquire
  admittance for a WMM AC

  @param pAdapter : [in]  pointer to adapter context
  @param acType   : [in]  WMM AC type of OS packet
  @param pGranted : [out] pointer to boolean flag when indicates if access
                          has been granted or not

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure
  ===========================================================================*/
VOS_STATUS hdd_wmm_acquire_access( hdd_adapter_t* pAdapter,
                                   WLANTL_ACEnumType acType,
                                   v_BOOL_t * pGranted );

/**============================================================================
  @brief hdd_wmm_assoc() - Function which will handle the housekeeping
  required by WMM when association takes place

  @param pAdapter : [in]  pointer to adapter context
  @param pRoamInfo: [in]  pointer to roam information
  @param eBssType : [in]  type of BSS

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure
  ===========================================================================*/
VOS_STATUS hdd_wmm_assoc( hdd_adapter_t* pAdapter,
                          tCsrRoamInfo *pRoamInfo,
                          eCsrRoamBssType eBssType );

/**============================================================================
  @brief hdd_wmm_connect() - Function which will handle the housekeeping
  required by WMM when a connection is established

  @param pAdapter : [in]  pointer to adapter context
  @param pRoamInfo: [in]  pointer to roam information
  @param eBssType : [in]  type of BSS

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure
  ===========================================================================*/
VOS_STATUS hdd_wmm_connect( hdd_adapter_t* pAdapter,
                            tCsrRoamInfo *pRoamInfo,
                            eCsrRoamBssType eBssType );

/**============================================================================
  @brief hdd_wmm_get_uapsd_mask() - Function which will calculate the
  initial value of the UAPSD mask based upon the device configuration

  @param pAdapter  : [in]  pointer to adapter context
  @param pUapsdMask: [in]  pointer to where the UAPSD Mask is to be stored

  @return         : VOS_STATUS_SUCCESS if succssful
                  : other values if failure
  ===========================================================================*/
VOS_STATUS hdd_wmm_get_uapsd_mask( hdd_adapter_t* pAdapter,
                                   tANI_U8 *pUapsdMask );

/**============================================================================
  @brief hdd_wmm_is_active() - Function which will determine if WMM is
  active on the current connection

  @param pAdapter  : [in]  pointer to adapter context

  @return         : VOS_TRUE if WMM is enabled
                  : VOS_FALSE if WMM is not enabled
  ===========================================================================*/
v_BOOL_t hdd_wmm_is_active( hdd_adapter_t* pAdapter );

/**============================================================================
  @brief hdd_wmm_addts() - Function which will add a traffic spec at the
  request of an application

  @param pAdapter  : [in]  pointer to adapter context
  @param handle    : [in]  handle to uniquely identify a TS
  @param pTspec    : [in]  pointer to the traffic spec

  @return          : HDD_WLAN_WMM_STATUS_* 
  ===========================================================================*/
hdd_wlan_wmm_status_e hdd_wmm_addts( hdd_adapter_t* pAdapter,
                                     v_U32_t handle,
                                     sme_QosWmmTspecInfo* pTspec );

/**============================================================================
  @brief hdd_wmm_delts() - Function which will delete a traffic spec at the
  request of an application

  @param pAdapter  : [in]  pointer to adapter context
  @param handle    : [in]  handle to uniquely identify a TS

  @return          : HDD_WLAN_WMM_STATUS_* 
  ===========================================================================*/
hdd_wlan_wmm_status_e hdd_wmm_delts( hdd_adapter_t* pAdapter,
                                     v_U32_t handle );

/**============================================================================
  @brief hdd_wmm_checkts() - Function which will return the status of a traffic
  spec at the request of an application

  @param pAdapter  : [in]  pointer to adapter context
  @param handle    : [in]  handle to uniquely identify a TS

  @return          : HDD_WLAN_WMM_STATUS_* 
  ===========================================================================*/
hdd_wlan_wmm_status_e hdd_wmm_checkts( hdd_adapter_t* pAdapter,
                                       v_U32_t handle );
/**============================================================================
  @brief hdd_wmm_adapter_clear() - Function which will clear the WMM status
  of all ACs
  @param pAdapter  : [in]  pointer to adapter context

  @return          : VOS_STATUS_SUCCESS if succssful
                   : other values if failure
  ===========================================================================*/
VOS_STATUS hdd_wmm_adapter_clear( hdd_adapter_t *pAdapter );

#endif /* #ifndef _WLAN_HDD_WMM_H */
