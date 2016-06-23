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




#ifndef WLAN_QCT_WDI_I_H
#define WLAN_QCT_WDI_I_H

/*===========================================================================

         W L A N   D E V I C E   A B S T R A C T I O N   L A Y E R 
              I N T E R N A L     A P I       F O R    T H E
                                 D A T A   P A T H 
                
                   
DESCRIPTION
  This file contains the internal API exposed by the DAL Control Path Core 
  module to be used by the DAL Data Path Core.
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
10/05/11    hap     Adding support for Keep Alive
08/19/10    lti     Created module.

===========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_api.h"
#include "wlan_qct_pal_list.h"
#include "wlan_qct_pal_sync.h"
#include "wlan_qct_pal_timer.h"
#include "wlan_qct_wdi_cts.h" 
#include "wlan_qct_wdi_bd.h" 

#include "wlan_hal_msg.h"
#include "wlan_status_code.h"
#include "wlan_qct_dev_defs.h"
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*Assert macro - redefined for WDI so it is more flexible in disabling*/
#define WDI_ASSERT(_cond) WPAL_ASSERT(_cond)

/*Error codes that can be returned by WDI - the HAL Error codes are not 
 propagated outside WDI because they are too explicit when refering to RIVA
 HW errors - they are masked under dev internal failure*/
#define WDI_ERR_BASIC_OP_FAILURE           0 
#define WDI_ERR_TRANSPORT_FAILURE          1 
#define WDI_ERR_INVALID_RSP_FMT            2 
#define WDI_ERR_RSP_TIMEOUT                3 
#define WDI_ERR_DEV_INTERNAL_FAILURE       4

/*In prima 12 HW stations are supported including BCAST STA(staId 0)
 and SELF STA(staId 1) so total ASSOC stations which can connect to Prima
 SoftAP = 12 - 1(Self STa) - 1(Bcast Sta) = 10 Stations. */
 
#ifdef WLAN_SOFTAP_VSTA_FEATURE
#define WDI_MAX_SUPPORTED_STAS   41
#else
#define WDI_MAX_SUPPORTED_STAS   12
#endif
#define WDI_MAX_SUPPORTED_BSS     5 

/* Control transport channel size*/
#define WDI_CT_CHANNEL_SIZE 4096

/*Invalid BSS index ! TO DO: Must come from the HAL header file*/
#define WDI_BSS_INVALID_IDX 0xFF

#define WDI_FTM_MAX_RECEIVE_BUFFER   6500

/*---------------------------------------------------------------------------
  DAL Control Path Main States
---------------------------------------------------------------------------*/      
typedef enum
{
  /* Transition in this state made upon creation and when a close request is 
     received*/
  WDI_INIT_ST = 0,      

  /* Transition happens after a Start response was received from HAL (as a
  result of a previously sent HAL Request)*/
  WDI_STARTED_ST,     

  /* Transition happens when a Stop request was received */
  WDI_STOPPED_ST, 

  /* Transition happens when a request is being sent down to HAL and we are
    waiting for the response */
  WDI_BUSY_ST,  

  /* Transition happens when 'SSR' shutdown request is recieved.*/
  WDI_SHUTDOWN_ST,

  WDI_MAX_ST
}WDI_MainStateType;


/*---------------------------------------------------------------------------
  DAL Control Path Scan States
---------------------------------------------------------------------------*/      
typedef enum
{
  /*The flag will be set to this state when init is called. Once the flag has   
    this value the only two scanning API calls allowed are Scan Start and 
    Scan Finished*/
  WDI_SCAN_INITIALIZED_ST = 0, 

  /*The flag will be set to this value once the Start Scan API is called.   
    When the flag has this value only Scan End API will be allowed. */
  WDI_SCAN_STARTED_ST     = 1, 

  /*The flag will be set to this value when End Scan API is called. When the   
    flag is set to this value the only two Scan APIs allowed are Start and 
    Finish. */
  WDI_SCAN_ENDED_ST       = 2, 

  /*The flag will be set to this value in the beginning before init is called   
    and after the Finish API is called. No other scan APIs will be allowed 
    in this state until Scan Init is called again. */
  WDI_SCAN_FINISHED_ST    = 3,

  WDI_SCAN_MAX_ST
}WDI_ScanStateType;

/*--------------------------------------------------------------------------- 
   WLAN DAL BSS Session Type - used to allow simulatneous association
   and keep track of each associated session 
 ---------------------------------------------------------------------------*/
#define WDI_MAX_BSS_SESSIONS  10

typedef enum
{
  /*Init state*/
  WDI_ASSOC_INIT_ST, 

  /*Joining State*/
  WDI_ASSOC_JOINING_ST, 

  /*Associated state*/
  WDI_ASSOC_POST_ST,

  WDI_ASSOC_MAX_ST
}WDI_AssocStateType;

/*--------------------------------------------------------------------------- 
   WLAN DAL Supported Request Types
 ---------------------------------------------------------------------------*/
typedef enum
{
  /*WLAN DAL START Request*/
  WDI_START_REQ  = 0, 

  /*WLAN DAL STOP Request*/
  WDI_STOP_REQ   = 1, 

  /*WLAN DAL STOP Request*/
  WDI_CLOSE_REQ  = 2, 


  /*SCAN*/
  /*WLAN DAL Init Scan Request*/
  WDI_INIT_SCAN_REQ    = 3, 

  /*WLAN DAL Start Scan Request*/
  WDI_START_SCAN_REQ   = 4, 

  /*WLAN DAL End Scan Request*/
  WDI_END_SCAN_REQ     = 5, 

  /*WLAN DAL Finish Scan Request*/
  WDI_FINISH_SCAN_REQ  = 6, 


  /*ASSOCIATION*/
  /*WLAN DAL Join Request*/
  WDI_JOIN_REQ          = 7, 

  /*WLAN DAL Config BSS Request*/
  WDI_CONFIG_BSS_REQ    = 8, 

  /*WLAN DAL Del BSS Request*/
  WDI_DEL_BSS_REQ       = 9, 

  /*WLAN DAL Post Assoc Request*/
  WDI_POST_ASSOC_REQ    = 10, 

  /*WLAN DAL Del STA Request*/
  WDI_DEL_STA_REQ       = 11, 

  /*Security*/
  /*WLAN DAL Set BSS Key Request*/
  WDI_SET_BSS_KEY_REQ   = 12,
  
  /*WLAN DAL Remove BSS Key Request*/ 
  WDI_RMV_BSS_KEY_REQ   = 13,
  
  /*WLAN DAL Set STA Key Request*/ 
  WDI_SET_STA_KEY_REQ   = 14,
  
  /*WLAN DAL Remove STA Key Request*/ 
  WDI_RMV_STA_KEY_REQ   = 15, 

  /*QOS and BA*/
  /*WLAN DAL Add TSpec Request*/
  WDI_ADD_TS_REQ        = 16,
  
  /*WLAN DAL Delete TSpec Request*/ 
  WDI_DEL_TS_REQ        = 17,
  
  /*WLAN DAL Update EDCA Params Request*/ 
  WDI_UPD_EDCA_PRMS_REQ = 18,

  /*WLAN DAL Add BA Session Request*/
  WDI_ADD_BA_SESSION_REQ = 19,

  /*WLAN DAL Delete BA Request*/ 
  WDI_DEL_BA_REQ        = 20,

   /* Miscellaneous Control */
  /*WLAN DAL Channel Switch Request*/ 
  WDI_CH_SWITCH_REQ     = 21,
  
  /*WLAN DAL Config STA Request*/ 
  WDI_CONFIG_STA_REQ    = 22, 

  /*WLAN DAL Set Link State Request*/ 
  WDI_SET_LINK_ST_REQ   = 23,
  
  /*WLAN DAL Get Stats Request*/ 
  WDI_GET_STATS_REQ     = 24, 

 /*WLAN DAL Update Config Request*/ 
  WDI_UPDATE_CFG_REQ    = 25, 

  /* WDI ADD BA Request */
  WDI_ADD_BA_REQ        = 26,

  /* WDI Trigger BA Request */
  WDI_TRIGGER_BA_REQ    = 27,

  /*WLAN DAL Update Beacon Params Request*/ 
  WDI_UPD_BCON_PRMS_REQ = 28,

  /*WLAN DAL Send Beacon template Request*/ 
  WDI_SND_BCON_REQ      = 29,

  /*WLAN DAL Send Probe Response Template Request*/ 
  WDI_UPD_PROBE_RSP_TEMPLATE_REQ = 30,

  /*WLAN DAL Set STA Bcast Key Request*/ 
  WDI_SET_STA_BCAST_KEY_REQ      = 31,
  
  /*WLAN DAL Remove STA Bcast Key Request*/ 
  WDI_RMV_STA_BCAST_KEY_REQ      = 32, 

  /*WLAN DAL Set Max Tx Power Request*/
  WDI_SET_MAX_TX_POWER_REQ       = 33,
  
  /* WLAN DAL P2P GO Notice Of Absence Request */
  WDI_P2P_GO_NOTICE_OF_ABSENCE_REQ    = 34,

  /*WLAN DAL Enter IMPS Request*/ 
  WDI_ENTER_IMPS_REQ    = 35, 

  /*WLAN DAL Exit IMPS Request*/ 
  WDI_EXIT_IMPS_REQ     = 36, 

  /*WLAN DAL Enter BMPS Request*/ 
  WDI_ENTER_BMPS_REQ    = 37, 

  /*WLAN DAL Exit BMPS Request*/ 
  WDI_EXIT_BMPS_REQ     = 38, 

  /*WLAN DAL Enter UAPSD Request*/ 
  WDI_ENTER_UAPSD_REQ   = 39, 

  /*WLAN DAL Exit UAPSD Request*/ 
  WDI_EXIT_UAPSD_REQ    = 40, 

  /*WLAN DAL Set UAPSD Param Request*/  
  WDI_SET_UAPSD_PARAM_REQ = 41, 

  /*WLAN DAL Update UAPSD Param (SoftAP mode) Request*/
  WDI_UPDATE_UAPSD_PARAM_REQ = 42, 

  /*WLAN DAL Configure RXP filter Request*/
  WDI_CONFIGURE_RXP_FILTER_REQ = 43, 

  /*WLAN DAL Configure Beacon filter Request*/
  WDI_SET_BEACON_FILTER_REQ = 44, 

  /*WLAN DAL Remove Beacon filter Request*/
  WDI_REM_BEACON_FILTER_REQ = 45, 

  /*WLAN DAL Set RSSI thresholds Request*/
  WDI_SET_RSSI_THRESHOLDS_REQ = 46, 

  /*WLAN DAL host offload Request*/
  WDI_HOST_OFFLOAD_REQ = 47, 

  /*WLAN DAL add wowl bc ptrn Request*/
  WDI_WOWL_ADD_BC_PTRN_REQ = 48, 

  /*WLAN DAL delete wowl bc ptrn Request*/
  WDI_WOWL_DEL_BC_PTRN_REQ = 49, 

  /*WLAN DAL enter wowl Request*/
  WDI_WOWL_ENTER_REQ = 50, 

  /*WLAN DAL exit wowl Request*/
  WDI_WOWL_EXIT_REQ = 51, 

  /*WLAN DAL Configure Apps CPU Wakeup state Request*/
  WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ = 52, 

  /* WLAN  NV Download Request */
  WDI_NV_DOWNLOAD_REQ   = 53,
  /*WLAN DAL Flush AC Request*/ 
  WDI_FLUSH_AC_REQ      = 54, 

  /*WLAN DAL BT AMP event Request*/ 
  WDI_BTAMP_EVENT_REQ   = 55, 
  /*WLAN DAL Aggregated Add TSpec Request*/
  WDI_AGGR_ADD_TS_REQ   = 56,

  WDI_ADD_STA_SELF_REQ       = 57,

  WDI_DEL_STA_SELF_REQ       = 58,

  /* WLAN FTM Command request */
  WDI_FTM_CMD_REQ            = 59,

  /*WLAN START OEM_DATA MEAS Request*/
  WDI_START_OEM_DATA_REQ   = 60,
  /* WLAN host resume request */
  WDI_HOST_RESUME_REQ      = 61,
  
  WDI_KEEP_ALIVE_REQ       = 62,  

  /* Set PNO */
  WDI_SET_PREF_NETWORK_REQ     = 63,

  /*RSSI Filter Request*/
  WDI_SET_RSSI_FILTER_REQ      = 64,

  /* Update Scan Parameters*/
  WDI_UPDATE_SCAN_PARAMS_REQ   = 65,

  WDI_SET_TX_PER_TRACKING_REQ = 66,

  WDI_8023_MULTICAST_LIST_REQ                   = 67,
  WDI_RECEIVE_FILTER_SET_FILTER_REQ             = 68,
  WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ  = 69,
  WDI_RECEIVE_FILTER_CLEAR_FILTER_REQ           = 70,
  
  /*This is temp fix. Should be removed once 
   * Host and Riva code is in sync*/
  WDI_INIT_SCAN_CON_REQ                         = 71, 

  /* WLAN HAL DUMP Command request */
  WDI_HAL_DUMP_CMD_REQ                          = 72,

  /* WLAN DAL Shutdown Request */
  WDI_SHUTDOWN_REQ                              = 73,

  /*Set power parameters on the device*/
  WDI_SET_POWER_PARAMS_REQ                      = 74,

  /* Traffic Stream Metrics statistic request */
  WDI_TSM_STATS_REQ                             = 75,
  /* GTK Rekey Offload */
  WDI_GTK_OFFLOAD_REQ             = 76, 
  WDI_GTK_OFFLOAD_GETINFO_REQ   = 77, 

  /*Set Thermal Migration level to RIVA*/
  WDI_SET_TM_LEVEL_REQ                          = 78, 

  /* Send a capability exchange message to HAL */
  WDI_FEATURE_CAPS_EXCHANGE_REQ                 = 79,

#ifdef WLAN_FEATURE_11AC
  /* Send a capability exchange message to HAL */
  WDI_UPDATE_VHT_OP_MODE_REQ                    = 80,
#endif

  /*WLAN DAL Get Roam Rssi Request*/
  WDI_GET_ROAM_RSSI_REQ                         = 81,

  /*WLAN DAL Set Tx Power Request*/
  WDI_SET_TX_POWER_REQ                          = 82,
  WDI_ROAM_SCAN_OFFLOAD_REQ                     = 83,

  WDI_TDLS_LINK_ESTABLISH_REQ                   = 84,

  /* WLAN FW LPHB config request */
  WDI_LPHB_CFG_REQ                              = 85,

  /* WLAN FW set batch scan request */
  WDI_SET_BATCH_SCAN_REQ                        = 86,

  /*WLAN DAL Set Max Tx Power Per band Request*/
  WDI_SET_MAX_TX_POWER_PER_BAND_REQ             = 87,

  WDI_UPDATE_CHAN_REQ                           = 88,

  WDI_GET_BCN_MISS_RATE_REQ                     = 89,

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
  WDI_LL_STATS_SET_REQ                          = 90,
  WDI_LL_STATS_GET_REQ                          = 91,
  WDI_LL_STATS_CLEAR_REQ                        = 92,
#endif

#ifdef WLAN_FEATURE_EXTSCAN
  WDI_EXTSCAN_START_REQ                          = 93,
  WDI_EXTSCAN_STOP_REQ                           = 94,
  WDI_EXTSCAN_GET_CACHED_RESULTS_REQ             = 95,
  WDI_EXTSCAN_GET_CAPABILITIES_REQ               = 96,
  WDI_EXTSCAN_SET_BSSID_HOTLIST_REQ              = 97,
  WDI_EXTSCAN_RESET_BSSID_HOTLIST_REQ            = 98,
  WDI_EXTSCAN_SET_SIGNF_RSSI_CHANGE_REQ          = 99,
  WDI_EXTSCAN_RESET_SIGNF_RSSI_CHANGE_REQ        = 100,
#endif

  WDI_SPOOF_MAC_ADDR_REQ                         = 101,

  WDI_GET_FW_STATS_REQ                           = 102,

  /* Send command to encrypt the given message */
  WDI_ENCRYPT_MSG_REQ                            = 103,

  WDI_MAX_REQ,

  /*Send a suspend Indication down to HAL*/
  WDI_HOST_SUSPEND_IND          = WDI_MAX_REQ ,

  /* Send a traffic stats indication to HAL */
  WDI_TRAFFIC_STATS_IND,

  /* DHCP Start Indication */
  WDI_DHCP_START_IND,

  /* DHCP Stop Indication */
  WDI_DHCP_STOP_IND,

  /* Drop/Receive unencrypted frames indication to HAL */
  WDI_EXCLUDE_UNENCRYPTED_IND,

  /* Send an add periodic Tx pattern indication to HAL */
  WDI_ADD_PERIODIC_TX_PATTERN_IND,

  /* Send a delete periodic Tx pattern indicationto HAL */
  WDI_DEL_PERIODIC_TX_PATTERN_IND,

  /*Send stop batch scan indication to FW*/
  WDI_STOP_BATCH_SCAN_IND,

  /*Send stop batch scan indication to FW*/
  WDI_TRIGGER_BATCH_SCAN_RESULT_IND,

  /* Send Rate Update Indication */
  WDI_RATE_UPDATE_IND,

  WDI_START_HT40_OBSS_SCAN_IND,
  WDI_STOP_HT40_OBSS_SCAN_IND,

  /* csa channel switch req*/
  WDI_CH_SWITCH_REQ_V1,
  WDI_TDLS_CHAN_SWITCH_REQ,

  /*Keep adding the indications to the max request
    such that we keep them sepparate */
  WDI_MAX_UMAC_IND
}WDI_RequestEnumType;

/*--------------------------------------------------------------------------- 
   WLAN DAL Supported Response Types
 ---------------------------------------------------------------------------*/
typedef enum
{
  /*WLAN DAL START Response*/
  WDI_START_RESP  = 0, 

  /*WLAN DAL STOP Response*/
  WDI_STOP_RESP   = 1, 

  /*WLAN DAL STOP Response*/
  WDI_CLOSE_RESP  = 2, 

  /*SCAN*/
  /*WLAN DAL Init Scan Response*/
  WDI_INIT_SCAN_RESP    = 3, 

  /*WLAN DAL Start Scan Response*/
  WDI_START_SCAN_RESP   = 4, 

  /*WLAN DAL End Scan Response*/
  WDI_END_SCAN_RESP     = 5, 

  /*WLAN DAL Finish Scan Response*/
  WDI_FINISH_SCAN_RESP  = 6, 


  /*ASSOCIATION*/
  /*WLAN DAL Join Response*/
  WDI_JOIN_RESP          = 7, 

  /*WLAN DAL Config BSS Response*/
  WDI_CONFIG_BSS_RESP    = 8, 

  /*WLAN DAL Del BSS Response*/
  WDI_DEL_BSS_RESP       = 9, 

  /*WLAN DAL Post Assoc Response*/
  WDI_POST_ASSOC_RESP    = 10, 

  /*WLAN DAL Del STA Response*/
  WDI_DEL_STA_RESP       = 11, 

  /*WLAN DAL Set BSS Key Response*/
  WDI_SET_BSS_KEY_RESP   = 12,
  
  /*WLAN DAL Remove BSS Key Response*/ 
  WDI_RMV_BSS_KEY_RESP   = 13,
  
  /*WLAN DAL Set STA Key Response*/ 
  WDI_SET_STA_KEY_RESP   = 14,
  
  /*WLAN DAL Remove STA Key Response*/ 
  WDI_RMV_STA_KEY_RESP   = 15, 

  /*WLAN DAL Add TSpec Response*/
  WDI_ADD_TS_RESP        = 16,
  
  /*WLAN DAL Delete TSpec Response*/ 
  WDI_DEL_TS_RESP        = 17,
  
  /*WLAN DAL Update EDCA Params Response*/ 
  WDI_UPD_EDCA_PRMS_RESP = 18,

  /*WLAN DAL Add BA Session Response*/
  WDI_ADD_BA_SESSION_RESP = 19,

  /*WLAN DAL Delete BA Response*/ 
  WDI_DEL_BA_RESP        = 20,

  /*WLAN DAL Channel Switch Response*/ 
  WDI_CH_SWITCH_RESP     = 21,
  
  /*WLAN DAL Config STA Response*/ 
  WDI_CONFIG_STA_RESP    = 22, 

  /*WLAN DAL Set Link State Response*/ 
  WDI_SET_LINK_ST_RESP   = 23,
  
  /*WLAN DAL Get Stats Response*/ 
  WDI_GET_STATS_RESP     = 24, 

  /*WLAN DAL Update Config Response*/ 
  WDI_UPDATE_CFG_RESP    = 25, 

  /* WDI ADD BA Response */
  WDI_ADD_BA_RESP        = 26,

  /* WDI Trigger BA Response */
  WDI_TRIGGER_BA_RESP    = 27,

 /*WLAN DAL Update beacon params  Response*/
  WDI_UPD_BCON_PRMS_RESP = 28,
  
 /*WLAN DAL Send beacon template Response*/
  WDI_SND_BCON_RESP = 29,

  /*WLAN DAL Update Probe Response Template Response*/
  WDI_UPD_PROBE_RSP_TEMPLATE_RESP = 30,

  /*WLAN DAL Set STA Key Response*/ 
  WDI_SET_STA_BCAST_KEY_RESP      = 31,
  
  /*WLAN DAL Remove STA Key Response*/ 
  WDI_RMV_STA_BCAST_KEY_RESP      = 32, 

  /*WLAN DAL Set Max Tx Power Response*/
  WDI_SET_MAX_TX_POWER_RESP       = 33,

  /*WLAN DAL Enter IMPS Response*/ 
  WDI_ENTER_IMPS_RESP     = 34, 

  /*WLAN DAL Exit IMPS Response*/ 
  WDI_EXIT_IMPS_RESP      = 35, 

  /*WLAN DAL Enter BMPS Response*/ 
  WDI_ENTER_BMPS_RESP    = 36, 

  /*WLAN DAL Exit BMPS Response*/ 
  WDI_EXIT_BMPS_RESP      = 37, 

  /*WLAN DAL Enter UAPSD Response*/ 
  WDI_ENTER_UAPSD_RESP    = 38, 

  /*WLAN DAL Exit UAPSD Response*/ 
  WDI_EXIT_UAPSD_RESP     = 39, 

  /*WLAN DAL Set UAPSD Param Response*/  
  WDI_SET_UAPSD_PARAM_RESP = 40, 

  /*WLAN DAL Update UAPSD Param (SoftAP mode) Response*/
  WDI_UPDATE_UAPSD_PARAM_RESP = 41, 

  /*WLAN DAL Configure RXP filter Response*/
  WDI_CONFIGURE_RXP_FILTER_RESP = 42, 

  /*WLAN DAL Set Beacon filter Response*/
  WDI_SET_BEACON_FILTER_RESP = 43, 

  /*WLAN DAL Remove Beacon filter Response*/
  WDI_REM_BEACON_FILTER_RESP = 44, 

  /*WLAN DAL Set RSSI thresholds Response*/
  WDI_SET_RSSI_THRESHOLDS_RESP = 45, 

  /*WLAN DAL Set RSSI thresholds Response*/
  WDI_HOST_OFFLOAD_RESP = 46, 

  /*WLAN DAL add wowl bc ptrn Response*/
  WDI_WOWL_ADD_BC_PTRN_RESP = 47, 

  /*WLAN DAL delete wowl bc ptrn Response*/
  WDI_WOWL_DEL_BC_PTRN_RESP = 48, 

  /*WLAN DAL enter wowl Response*/
  WDI_WOWL_ENTER_RESP = 49, 

  /*WLAN DAL exit wowl Response*/
  WDI_WOWL_EXIT_RESP = 50, 

  /*WLAN DAL Configure Apps CPU Wakeup state Response*/
  WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_RESP = 51,
  
  /* WLAN NV Download responce */
  WDI_NV_DOWNLOAD_RESP = 52,

  /*WLAN DAL Flush AC Response*/ 
  WDI_FLUSH_AC_RESP      = 53,

  /*WLAN DAL Flush AC Response*/ 
  WDI_BTAMP_EVENT_RESP   = 54,

  /*WLAN DAL Add Aggregated TSpec Response*/
  WDI_AGGR_ADD_TS_RESP  = 55,

  /*Add Self STA Response*/
  WDI_ADD_STA_SELF_RESP = 56,
  
  /*Delete Self STA Response*/
  WDI_DEL_STA_SELF_RESP       = 57,

  /*WLAN START OEM_DATA Response*/
  WDI_START_OEM_DATA_RESP   = 58,

  /* WLAN host resume request */
  WDI_HOST_RESUME_RESP        = 59,

  /* WLAN DAL P2P GO Notice Of Absence Response */
  WDI_P2P_GO_NOTICE_OF_ABSENCE_RESP    = 60,

  /* FTM Response from HAL */
  WDI_FTM_CMD_RESP                     = 61,

  /*Keep alive response */
  WDI_KEEP_ALIVE_RESP                  = 62,

  /* Set PNO Response */
  WDI_SET_PREF_NETWORK_RESP            = 63,

  /* Set RSSI Filter Response */
  WDI_SET_RSSI_FILTER_RESP             = 64,

  /* Update Scan Parameters Resp */
  WDI_UPDATE_SCAN_PARAMS_RESP          = 65,

  //Tx PER Tracking
  WDI_SET_TX_PER_TRACKING_RESP       = 66,


  
  /* Packet Filtering Response */
  WDI_8023_MULTICAST_LIST_RESP                  = 67,

  WDI_RECEIVE_FILTER_SET_FILTER_RESP            = 68,

  WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_RESP = 69,

  WDI_RECEIVE_FILTER_CLEAR_FILTER_RESP          = 70,

  
  /* WLAN HAL DUMP Command Response */
  WDI_HAL_DUMP_CMD_RESP                         = 71,

  /* WLAN Shutdown Response */
  WDI_SHUTDOWN_RESP                             = 72,

  /*Set power parameters response */
  WDI_SET_POWER_PARAMS_RESP                     = 73,

  WDI_TSM_STATS_RESP                            = 74,
  /* GTK Rekey Offload */
  WDI_GTK_OFFLOAD_RESP                          = 75, 
  WDI_GTK_OFFLOAD_GETINFO_RESP                  = 76, 

  WDI_SET_TM_LEVEL_RESP                         = 77,

  /* FW sends its capability bitmap as a response */
  WDI_FEATURE_CAPS_EXCHANGE_RESP                = 78,

#ifdef WLAN_FEATURE_11AC
  WDI_UPDATE_VHT_OP_MODE_RESP                   = 79,
#endif

  /* WLAN DAL Get Roam Rssi Response*/
  WDI_GET_ROAM_RSSI_RESP                        = 80,

  WDI_SET_TX_POWER_RESP                         = 81,
  WDI_ROAM_SCAN_OFFLOAD_RESP                    = 82,

  WDI_TDLS_LINK_ESTABLISH_REQ_RESP              = 83,

  /* WLAN FW LPHB Config response */
  WDI_LPHB_CFG_RESP                             = 84,

  WDI_SET_BATCH_SCAN_RESP                       = 85,

  WDI_SET_MAX_TX_POWER_PER_BAND_RSP             = 86,

  WDI_UPDATE_CHAN_RESP                          = 87,
  /* channel switch resp v1*/
  WDI_CH_SWITCH_RESP_V1                         = 88,

  WDI_GET_BCN_MISS_RATE_RSP                     = 89,
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
  WDI_LL_STATS_SET_RSP                          = 90,
  WDI_LL_STATS_GET_RSP                          = 91,
  WDI_LL_STATS_CLEAR_RSP                        = 92,
#endif

#ifdef WLAN_FEATURE_EXTSCAN
  WDI_EXTSCAN_START_RSP                          = 93,
  WDI_EXTSCAN_STOP_RSP                           = 94,
  WDI_EXTSCAN_GET_CACHED_RESULTS_RSP             = 95,
  WDI_EXTSCAN_GET_CAPABILITIES_RSP               = 96,
  WDI_EXTSCAN_SET_HOTLIST_BSSID_RSP              = 97,
  WDI_EXTSCAN_RESET_HOTLIST_BSSID_RSP            = 98,
  WDI_EXTSCAN_SET_SIGNF_RSSI_CHANGE_RSP          = 99,
  WDI_EXTSCAN_RESET_SIGNF_RSSI_CHANGE_RSP        = 100,
#endif
  WDI_SPOOF_MAC_ADDR_RSP                         = 101,
  WDI_GET_FW_STATS_RSP                           = 102,

  /* Send command to encrypt the given message */
  WDI_ENCRYPT_MSG_RSP                            = 103,
  /*-------------------------------------------------------------------------
    Indications
     !! Keep these last in the enum if possible
    -------------------------------------------------------------------------*/
  WDI_HAL_IND_MIN                     , 
  /*When RSSI monitoring is enabled of the Lower MAC and a threshold has been
    passed. */
  WDI_HAL_RSSI_NOTIFICATION_IND       = WDI_HAL_IND_MIN, 

  /*Link loss in the low MAC */
  WDI_HAL_MISSED_BEACON_IND           = WDI_HAL_IND_MIN + 1,

  /*When hardware has signaled an unknown addr2 frames. The indication will
  contain info from frames to be passed to the UMAC, this may use this info to
  deauth the STA*/
  WDI_HAL_UNKNOWN_ADDR2_FRAME_RX_IND  = WDI_HAL_IND_MIN + 2,

  /*MIC Failure detected by HW*/
  WDI_HAL_MIC_FAILURE_IND             = WDI_HAL_IND_MIN + 3,

  /*Fatal Error Ind*/
  WDI_HAL_FATAL_ERROR_IND             = WDI_HAL_IND_MIN + 4, 

  /*Received when the RIVA SW decides to autonomously delete an associate
    station (e.g. Soft AP TIM based dissassoc) */
  WDI_HAL_DEL_STA_IND                 = WDI_HAL_IND_MIN + 5,

  /*Coex indication*/
  WDI_HAL_COEX_IND                    = WDI_HAL_IND_MIN + 6,

  /* Tx Complete Indication */
  WDI_HAL_TX_COMPLETE_IND             = WDI_HAL_IND_MIN + 7,

  WDI_HAL_P2P_NOA_ATTR_IND            = WDI_HAL_IND_MIN + 8,

  /* Preferred Network Found Indication */
  WDI_HAL_PREF_NETWORK_FOUND_IND      = WDI_HAL_IND_MIN + 9,

  /* Wakeup Reason Indication */
  WDI_HAL_WAKE_REASON_IND              = WDI_HAL_IND_MIN + 10,

  /* Tx PER Hit Indication */
  WDI_HAL_TX_PER_HIT_IND              = WDI_HAL_IND_MIN + 11,

  /* NOA Start Indication from FW to Host */
  WDI_HAL_P2P_NOA_START_IND            = WDI_HAL_IND_MIN + 12,

  /* TDLS Indication from FW to Host */
  WDI_HAL_TDLS_IND                     = WDI_HAL_IND_MIN + 13,

  /* LPHB timeout indication */
  WDI_HAL_LPHB_IND                     = WDI_HAL_IND_MIN + 14,

  /* IBSS Peer Inactivity Indication from FW to Host */
  WDI_HAL_IBSS_PEER_INACTIVITY_IND     = WDI_HAL_IND_MIN + 15,

  /* Periodic Tx Pattern Indication from FW to Host */
  WDI_HAL_PERIODIC_TX_PTRN_FW_IND     = WDI_HAL_IND_MIN + 16,


  WDI_BATCHSCAN_RESULT_IND           =  WDI_HAL_IND_MIN + 17,

  WDI_HAL_CH_AVOID_IND                 = WDI_HAL_IND_MIN + 18,

  /* print register values indication from FW to Host */
  WDI_PRINT_REG_INFO_IND               = WDI_HAL_IND_MIN + 19,
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
  WDI_HAL_LL_STATS_RESULTS_IND         = WDI_HAL_IND_MIN + 20,
#endif
#ifdef WLAN_FEATURE_EXTSCAN
  WDI_HAL_EXTSCAN_PROGRESS_IND       = WDI_HAL_IND_MIN + 21,
  WDI_HAL_EXTSCAN_SCAN_AVAILABLE_IND = WDI_HAL_IND_MIN + 22,
  WDI_HAL_EXTSCAN_RESULT_IND         = WDI_HAL_IND_MIN + 23,
  WDI_HAL_EXTSCAN_BSSID_HOTLIST_RESULT_IND    = WDI_HAL_IND_MIN + 24,
  WDI_HAL_EXTSCAN_SIG_RSSI_RESULT_IND         = WDI_HAL_IND_MIN + 25,
#endif
  WDI_TDLS_CHAN_SWITCH_REQ_RESP      = WDI_HAL_IND_MIN + 26,
  WDI_HAL_DEL_BA_IND                 = WDI_HAL_IND_MIN + 27,
  WDI_MAX_RESP
}WDI_ResponseEnumType; 

typedef struct 
{
  /*Flag that marks a session as being in use*/
  wpt_boolean         bInUse; 

  /*Flag that keeps track if a series of assoc requests for this BSS are
    currently pending in the queue or processed
    - the flag is set to true when the Join request ends up being queued
    - and reset to false when the Pending queue is empty */
  wpt_boolean         bAssocReqQueued;

  /*BSSID of the session*/
  wpt_macAddr         macBSSID; 

  /*BSS Index associated with this BSSID*/
  wpt_uint8           ucBSSIdx; 

  /*Associated state of the current BSS*/
  WDI_AssocStateType  wdiAssocState;

  /*WDI Pending Request Queue*/
  wpt_list            wptPendingQueue;

  /*DPU Information for this BSS*/
  wpt_uint8           bcastDpuIndex;
  wpt_uint8           bcastDpuSignature;
  wpt_uint8           bcastMgmtDpuIndex;
  wpt_uint8           bcastMgmtDpuSignature;

  /*RMF enabled/disabled*/
  wpt_uint8           ucRmfEnabled;

  /*Bcast STA ID associated with this BSS session */
  wpt_uint8           bcastStaIdx;

  /*The type of the BSS in the session */
  WDI_BssType         wdiBssType;
}WDI_BSSSessionType;

/*---------------------------------------------------------------------------
  WDI_ConfigBSSRspInfoType
---------------------------------------------------------------------------*/
typedef WPT_PACK_PRE struct 
{
  /*BSS index allocated by HAL*/
  wpt_uint8    ucBSSIdx;

  /*BSSID of the BSS*/
  wpt_macAddr  macBSSID; 

  /*Broadcast DPU descriptor index allocated by HAL and used for
  broadcast/multicast packets.*/
  wpt_uint8    ucBcastDpuDescIndx;

  /*DPU signature to be used for broadcast/multicast packets*/
  wpt_uint8    ucBcastDpuSignature;
  
  /*DPU descriptor index allocated by HAL, used for bcast/mcast management
  packets*/
  wpt_uint8    ucMgmtDpuDescIndx;

  /*DPU signature to be used for bcast/mcast management packets*/
  wpt_uint8    ucMgmtDpuSignature;

  /*Status of the request received from HAL */
  eHalStatus   halStatus;
}WPT_PACK_POST WDI_ConfigBSSRspInfoType;


/*---------------------------------------------------------------------------
  WDI_PostAssocRspInfoType
---------------------------------------------------------------------------*/
typedef WPT_PACK_PRE struct
{
  /*STA Index allocated by HAL.*/
  wpt_uint16   usSTAIdx;
  
  /*MAC Address of STA*/ 
  wpt_macAddr  macSTA;
  
  /*Unicast DPU signature*/
  wpt_uint8    ucUcastSig;

  /*Broadcast DPU Signature*/
  wpt_uint8    ucBcastSig;

  /*BSSID of the BSS*/
  wpt_macAddr  macBSSID; 

  /*HAL Status */
  eHalStatus   halStatus;
}WPT_PACK_POST WDI_PostAssocRspInfoType;

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
/*---------------------------------------------------------------------------
  WDI_LLStatsResultsType
---------------------------------------------------------------------------*/
typedef WPT_PACK_PRE struct
{
   wpt_uint32 param_id;
   wpt_uint8  iface_id;
   wpt_uint32 resp_id;
   wpt_uint32 more_result_to_follow;
   wpt_uint8  result[1];
}WPT_PACK_POST WDI_LLstatsResultsType;

#endif

/*--------------------------------------------------------------------------- 
   WLAN DAL FSM Event Info Type 
 ---------------------------------------------------------------------------*/
typedef struct
{
  /*Events can be linked in a list - put a node in front for that, it will be
   used by wpt to link them*/
  wpt_list_node          wptListNode; 

  /*Request Received */
  WDI_RequestEnumType    wdiRequest;

  /*Response Received */
  WDI_ResponseEnumType   wdiResponse;

  /*Data associated with the request */
  void*                  pEventData;

  /*Data Size*/
  wpt_uint32             uEventDataSize;

  /*Callback function for receiving the response to the event*/
  void*                  pCBfnc;

  /*User data to be sent along with the CB function call*/
  void*                  pUserData;
}WDI_EventInfoType; 

/*--------------------------------------------------------------------------- 
   WLAN DAL Session Index Type
 ---------------------------------------------------------------------------*/
typedef struct
{
  /*Events can be linked in a list - put a node in front for that, it will be
   used by wpt to link them*/
  wpt_list_node          wptListNode; 

  /*Session id for the new association to be processed*/
  wpt_uint8              ucIndex; 

}WDI_NextSessionIdType; 

#define WDI_CONTROL_BLOCK_MAGIC  0x67736887  /* WDIC in little endian */
/*--------------------------------------------------------------------------- 
   WLAN DAL Control Block Type 
 ---------------------------------------------------------------------------*/
typedef struct
{
  /*magic number so callbacks can validate their context pointers*/
  wpt_uint32                  magic;

  /*Ptr to the OS Context received from the UMAC*/
  void*                       pOSContext;

  /*Ptr to the PAL Context received from PAL*/
  void*                       pPALContext; 

  /*Ptr to the Datapath Context received from PAL*/
  void*                       pDPContext; 

  /*Ptr to the Datapath Transport Driver Context received from PAL*/
  void*                       pDTDriverContext; 

  /*Hanlde to the control transport service*/
  WCTS_HandleType             wctsHandle;

  /*Flag that keeps track if CT is Opened or not*/
  wpt_boolean                 bCTOpened;

  /*The global state of the DAL Control Path*/
  WDI_MainStateType           uGlobalState; 

  /*Flag to keep track of the expected state transition after processing
    of a response */
  WDI_MainStateType           ucExpectedStateTransition;

  /*Main Synchronization Object for the WDI CB*/
  wpt_mutex                   wptMutex;

  /*WDI response timer*/
  wpt_timer                   wptResponseTimer;

  /*WDI Pending Request Queue*/
  wpt_list                    wptPendingQueue;
#if 0
  /*The state of the DAL during a scanning procedure*/
  WDI_ScanStateType           uScanState;  

  /*Flag that keeps track if a Scan is currently in progress*/
  wpt_boolean                 bScanInProgress;
#endif
  /*Flag that keeps track if an Association is currently in progress*/
  wpt_boolean                 bAssociationInProgress; 

  /*Array of simultaneous BSS Sessions*/
  WDI_BSSSessionType          aBSSSessions[WDI_MAX_BSS_SESSIONS];

  /*WDI Pending Association Session Id Queue - it keeps track of the
    order in which queued assoc requests came in*/
  wpt_list                    wptPendingAssocSessionIdQueue;

  /*! TO DO : - group these in a union, only one cached req can exist at a
      time  */

  /*Cached post assoc request - there can only be one in the system as
    only one request goes down to hal up until a response is received
    The values cached are used on response to save a station if needed */
  WDI_PostAssocReqParamsType  wdiCachedPostAssocReq; 

  /*Cached config sta request - there can only be one in the system as
    only one request goes down to hal up until a response is received
    The values cached are used on response to save a station if needed */
  WDI_ConfigSTAReqParamsType  wdiCachedConfigStaReq; 

  /*Cached config sta request - there can only be one in the system as
    only one request goes down to hal up until a response is received
    The values cached are used on response to save a BSS if needed */
  WDI_ConfigBSSReqParamsType  wdiCachedConfigBssReq; 

  /*Cached set link state request - there can only be one in the system as
    only one request goes down to hal up until a response is received
    The values cached are used on response to delete a BSS if needed */
  WDI_SetLinkReqParamsType     wdiCacheSetLinkStReq; 

  /*Cached add STA self request - there can only be one in the system as
    only one request goes down to hal up until a response is received
    The values cached are used on response to save the self STA in the table */
  WDI_AddSTASelfReqParamsType  wdiCacheAddSTASelfReq;

  /*Current session being handled*/
  wpt_uint8                   ucCurrentBSSSesIdx;

  /*Pointer to the response CB of the pending request*/
  void*                       pfncRspCB;

  /*Pointer to the user data to be sent along with the response CB*/
  void*                       pRspCBUserData; 

  /*The expected response from HAL*/
  WDI_ResponseEnumType        wdiExpectedResponse;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb             wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                       pReqStatusUserData;

   /*Indication callback given by UMAC to be called by the WLAN DAL when it
    wishes to send something back independent of a request*/
  WDI_LowLevelIndCBType       wdiLowLevelIndCB; 

  /*The user data passed in by UMAC, it will be sent back when the indication
    function pointer will be called */
  void*                       pIndUserData;

  /*Cached start response parameters*/
  WDI_StartRspParamsType      wdiCachedStartRspParams;
  
  /* Information related to NV Image*/
  WDI_NvBlobInfoParams        wdiNvBlobInfo;

  /*STA Table Information*/
  /*Max number of stations allowed by device */
  wpt_uint8                   ucMaxStations;

  /*Max number of BSSes allowed by device */
  wpt_uint8                   ucMaxBssids;

  /* Global BSS and STA table -  Memory is allocated when needed.*/
  void*                       staTable;

#ifndef HAL_SELF_STA_PER_BSS
  /*Index of the Self STA */
  wpt_uint8                   ucSelfStaId;

  /* Self STA DPU Index */
  wpt_uint16                  usSelfStaDpuId;

  /*Self STA Mac*/
  wpt_macAddr                 macSelfSta;
#endif

  /*Is frame translation enabled */
  wpt_uint8                   bFrameTransEnabled;

  /*AMSDU BD Fix Mask - used by the Fixing routine for Data Path */
  WDI_RxBdType                wdiRxAmsduBdFixMask;

  /*First AMSDU BD - used by the Fixing routine for Data Path */
  WDI_RxBdType                wdiRxAmsduFirstBdCache;

  /*This must be incremented on sta change */
  wpt_uint32                  uBdSigSerialNum;

  /* dpu routing flag
  ! TO DO: - must be set/reset when PS is enabled for UAPSD */  
  wpt_uint8                   ucDpuRF;    
  /* Event to wait for when WCTS is told to perform an action */
  wpt_event                   wctsActionEvent;
  /* Event to wait for ACK from DXE after the power state is set */
  wpt_event                   setPowerStateEvent;
  /* DXE physical addr to be passed down to RIVA. RIVA HAL will use it to program
  DXE when DXE wakes up from power save*/
  unsigned int                dxePhyAddr;

  wpt_boolean                 dxeRingsEmpty;

  /*NV download request parameters  */
  WDI_NvDownloadReqParamsType   wdiCachedNvDownloadReq;

  /* Driver Type */
  tDriverType                 driverMode;  

  /* Statically allocated FTM Response Buffer */
  wpt_uint8                   ucFTMCommandRspBuffer[WDI_FTM_MAX_RECEIVE_BUFFER];

  /*Driver in BMPS state*/
  wpt_boolean                 bInBmps;

  /*version of the PNO implementation in RIVA*/
  wpt_uint8                   wdiPNOVersion;

  /*SSR timer*/
  wpt_timer                   ssrTimer;

  /*Version of the WLAN HAL API received on start resp*/
  WDI_WlanVersionType wlanVersion;

  /*timestamp when we start response timer*/
  wpt_uint32                  uTimeStampRspTmrStart;

  /*timestamp when we get response timer event*/
  wpt_uint32                  uTimeStampRspTmrExp;

  /* enable/disable SSR on WDI timeout */
  wpt_boolean                 bEnableSSR;

  /* timestamp derived from msm arch counter. */
  /*timestamp when we start response timer*/
  wpt_uint64                  uArchTimeStampRspTmrStart;

  /*timestamp when we get response timer event*/
  wpt_uint64                  uArchTimeStampRspTmrExp;

  /* reason for WDI_DetectedDeviceError */
  void *                        DeviceErrorReason;
}WDI_ControlBlockType; 




/*---------------------------------------------------------------------------

  DESCRIPTION 
    WLAN DAL Request Processing function definition. 
    
  PARAMETERS 

   IN
   pWDICtx:         pointer to the WLAN DAL context 
   pEventData:      pointer to the event information structure 
 
   
  RETURN VALUE
    The result code associated with performing the operation  

---------------------------------------------------------------------------*/
typedef WDI_Status (*WDI_ReqProcFuncType)( WDI_ControlBlockType*  pWDICtx,
                                           WDI_EventInfoType*     pEventData);


/*---------------------------------------------------------------------------

  DESCRIPTION 
    WLAN DAL Response Processing function definition. 
    
  PARAMETERS 

   IN
   pWDICtx:         pointer to the WLAN DAL context 
   pEventData:      pointer to the event information structure 
 
   
  RETURN VALUE
    The result code associated with performing the operation  

---------------------------------------------------------------------------*/
typedef WDI_Status (*WDI_RspProcFuncType)( WDI_ControlBlockType*  pWDICtx,
                                           WDI_EventInfoType*     pEventData);




/*==========================================================================
              MAIN DAL FSM Definitions and Declarations 
==========================================================================*/

/*--------------------------------------------------------------------------- 
   DAL Control Path Main FSM  
 ---------------------------------------------------------------------------*/
#define WDI_STATE_TRANSITION(_pctx, _st)   (_pctx->uGlobalState = _st)



/*---------------------------------------------------------------------------
  DAL Main Event type
---------------------------------------------------------------------------*/      
typedef enum
{
  /* Start request received from UMAC */
  WDI_START_EVENT          = 0,

  /* Stop request received from UMAC */
  WDI_STOP_EVENT           = 1,

  /* HAL request received from UMAC*/
  WDI_REQUEST_EVENT        = 2,

  /* HAL Response received from device */
  WDI_RESPONSE_EVENT       = 3,

  /* Close request received from UMAC */
  WDI_CLOSE_EVENT          = 4,

  /* Shutdown request received from UMAC */
  WDI_SHUTDOWN_EVENT       = 5,

  WDI_MAX_EVENT

}WDI_MainEventType;

/*---------------------------------------------------------------------------

  DESCRIPTION 
    Main DAL state machine function definition. 
    
  PARAMETERS 

   IN
   pWDICtx:         pointer to the WLAN DAL context 
   pEventData:      pointer to the event information structure 
 
   
  RETURN VALUE
    The result code associated with performing the operation  

---------------------------------------------------------------------------*/
typedef WDI_Status (*WDI_MainFuncType)( WDI_ControlBlockType*  pWDICtx,
                                        WDI_EventInfoType*     pEventData);

/*---------------------------------------------------------------------------
  MAIN DAL FSM Entry type
---------------------------------------------------------------------------*/      
typedef struct
{
  WDI_MainFuncType  pfnMainTbl[WDI_MAX_EVENT];
} WDI_MainFsmEntryType;

/*Macro to check for valid session id*/
#define WDI_VALID_SESSION_IDX(_idx)  ( _idx < WDI_MAX_BSS_SESSIONS ) 

/*========================================================================== 
 
                      DAL INTERNAL FUNCTION DECLARATION
 
==========================================================================*/ 

/**
 @brief Helper routine for retrieving the PAL Context from WDI - 
        can be used by CTS, DTS, DXE and othe DAL internals 
 
 @param  None
  
 @see
 @return pointer to the context 
*/
void* WDI_GET_PAL_CTX( void );

/*---------------------------------------------------------------------------
                    MAIN DAL FSM Function Declarations
---------------------------------------------------------------------------*/      
/**
 @brief WDI_PostMainEvent - Posts an event to the Main FSM

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         wdiEV:           event posted to the main DAL FSM
         pEventData:      pointer to the event information
         structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_PostMainEvent
(
  WDI_ControlBlockType*  pWDICtx, 
  WDI_MainEventType      wdiEV, 
  WDI_EventInfoType*     pEventData
  
);

/*--------------------------------------------------------------------------
  INIT State Functions 
--------------------------------------------------------------------------*/
/**
 @brief Main FSM Start function for all states except BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStart
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Main FSM Response function for state INIT

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainRspInit
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Main FSM Close function for all states except BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
        
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainClose
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/*--------------------------------------------------------------------------
  STARTED State Functions 
--------------------------------------------------------------------------*/
/**
 @brief Main FSM Start function for state STARTED

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
      
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStartStarted
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Main FSM Stop function for state STARTED

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
         uEventDataSize:  size of the data sent in event
         pCBfnc:          cb function for event response
         pUserData:       user data 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStopStarted
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Main FSM Request function for state started

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
        
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainReqStarted
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Main FSM Response function for all states except INIT

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/*--------------------------------------------------------------------------
  STOPPED State Functions 
--------------------------------------------------------------------------*/
/**
 @brief Main FSM Stop function for state STOPPED

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStopStopped
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
  );

/*--------------------------------------------------------------------------
  BUSY State Functions 
--------------------------------------------------------------------------*/
/**
 @brief Main FSM Start function for state BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStartBusy
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Main FSM Stop function for state BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStopBusy
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Main FSM Request function for state BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainReqBusy
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Main FSM Close function for state BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainCloseBusy
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Main FSM Shutdown function for INIT & STARTED states


 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainShutdown
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Main FSM Shutdown function for BUSY state


 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainShutdownBusy
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/*========================================================================
          Main DAL Control Path Request Processing API 
========================================================================*/

/**
 @brief Process Start Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStartReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Stop Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStopReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Close Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessCloseReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Shutdown Request function (called when Main FSM
        allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessShutdownReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Init Scan Request function (called when Main FSM
        allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessInitScanReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Start Scan Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStartScanReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process End Scan Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEndScanReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Finish Scan Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFinishScanReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Join Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessJoinReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Config BSS Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigBSSReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Del BSS Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelBSSReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Post Assoc Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessPostAssocReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Del STA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelSTAReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set BSS Key Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetBssKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Remove BSS Key Request function (called when Main    
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveBssKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set STA KeyRequest function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetStaKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Remove STA Key Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveStaKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set STA KeyRequest function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetStaBcastKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Remove STA Key Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveStaBcastKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Add TSpec Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddTSpecReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Del TSpec Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelTSpecReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Update EDCA Params Request function (called when
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateEDCAParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Add BA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddBASessionReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Del BA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelBAReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef FEATURE_WLAN_ESE
/**
 @brief Process TSM Stats Request function (called when Main FSM
        allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTSMStatsReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif

/**
 @brief Process Channel Switch Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessChannelSwitchReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Channel Switch Request function (called when
        Main FSM allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/

WDI_Status WDI_ProcessChannelSwitchReq_V1
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Config STA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigStaReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Set Link State Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetLinkStateReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Get Stats Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessGetStatsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
/**
 @brief Process Get Roam rssi Request function (called when Main FSM
        allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessGetRoamRssiReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Get Roam Rssi Rsp function (called when a response is
        being received over the bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessGetRoamRssiRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#endif


/**
 @brief Process Update Cfg Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateCfgReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Add BA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddBAReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Trigger BA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTriggerBAReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Update Beacon Params Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateBeaconParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Update Beacon template Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSendBeaconParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Update Beacon Params  Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateProbeRspTemplateReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
/**
 @brief Process NV blob download function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessNvDownloadReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set Max Tx Power Request function (called when Main    
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status WDI_ProcessSetMaxTxPowerReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set Max Tx Power Per Band Request function (called when Main
        FSM allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status WDI_ProcessSetMaxTxPowerPerBandReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set Tx Power Request function (called when Main
        FSM allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status WDI_ProcessSetTxPowerReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process P2P Notice Of Absence Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessP2PGONOAReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process TDLS Link Establish Request function (called when Main FSM
        allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTdlsLinkEstablishReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process tdls channel switch request

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTdlsChanSwitchReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
/**
 @brief Process Enter IMPS Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterImpsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Exit IMPS Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitImpsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Enter BMPS Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterBmpsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Exit BMPS Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitBmpsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Enter UAPSD Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterUapsdReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Exit UAPSD Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitUapsdReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set UAPSD params Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetUapsdAcParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process update UAPSD params Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateUapsdParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Configure RXP filter Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigureRxpFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process set beacon filter Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetBeaconFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process remove beacon filter Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemBeaconFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process set RSSI thresholds Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetRSSIThresholdsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process set RSSI thresholds Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHostOffloadReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Keep Alive Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessKeepAliveReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Wowl add bc ptrn Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlAddBcPtrnReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Wowl delete bc ptrn Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlDelBcPtrnReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Wowl enter Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlEnterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Wowl exit Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlExitReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Configure Apps Cpu Wakeup State Request function
        (called when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigureAppsCpuWakeupStateReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Flush AC Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFlushAcReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef FEATURE_OEM_DATA_SUPPORT
/**
 @brief Process Start Oem Data Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStartOemDataReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif

/**
 @brief Process Host Resume Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHostResumeReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process BT AMP event Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessBtAmpEventReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Add STA self Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddSTASelfReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Del Sta Self Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelSTASelfReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process set Tx Per Tracking configurations Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetTxPerTrackingReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set Power Params Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetPowerParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set Thermal Mitigation level Changed request
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetTmLevelReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef FEATURE_WLAN_LPHB
/**
 @brief WDI_ProcessLPHBConfReq -
    LPHB configuration request to FW

 @param  pWDICtx : wdi context
         pEventData : indication data

 @see
 @return esult of the function call
*/
WDI_Status WDI_ProcessLPHBConfReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif /* FEATURE_WLAN_LPHB */

/**
 @brief WDI_ProcessUpdateChannelParamsReq -
    Send update channel request to FW

 @param  pWDICtx : wdi context
         pEventData : indication data

 @see
 @return success or failure
*/
WDI_Status WDI_ProcessUpdateChannelParamsReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef FEATURE_WLAN_BATCH_SCAN
/**
 @brief WDI_ProcessSetBatchScanReq -
    Send set batch scan configuration request to FW

 @param  pWDICtx : wdi context
         pEventData : indication data

 @see
 @return success or failure
*/
WDI_Status WDI_ProcessSetBatchScanReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief WDI_ProcessGetBatchScanReq -
    Send get batch scan request to FW

 @param  pWDICtx : wdi context
         pEventData : indication data

 @see
 @return success or failure
*/
WDI_Status WDI_ProcessGetBatchScanReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif /* FEATURE_WLAN_BATCH_SCAN */


/*=========================================================================
                             Indications
=========================================================================*/

/**
 @brief Process Suspend Indications function (called when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHostSuspendInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief DHCP Start Event Indication

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDHCPStartInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief DHCP Stop Event Indication

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDHCPStopInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Traffic Stats Indications function (called when Main FSM allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTrafficStatsInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef WLAN_FEATURE_11W
/**
 @brief Process Exclude Unencrypted Indications function (called
        when Main FSM allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
              pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExcludeUnencryptInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif

/**
 @brief Process Add Periodic Tx Pattern Indication function (called when
           Main FSM allows it)

 @param  pWDICtx:        pointer to the WLAN DAL context
         pEventData:     pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddPeriodicTxPtrnInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Delete Periodic Tx Pattern Indication function (called when
           Main FSM allows it)

 @param  pWDICtx:        pointer to the WLAN DAL context
         pEventData:     pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelPeriodicTxPtrnInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef FEATURE_WLAN_BATCH_SCAN
/**
  @brief Process stop batch scan indications function
         It is called when Main FSM allows it

  @param  pWDICtx:         pointer to the WLAN DAL context
          pEventData:      pointer to the event information structure

  @see
  @return Result of the function call
 */
 WDI_Status
 WDI_ProcessStopBatchScanInd
 (
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
 );

/**
  @brief This API is called to trigger batch scan results from FW
         It is called when Main FSM allows it

  @param  pWDICtx:         pointer to the WLAN DAL context
          pEventData:      pointer to the event information structure

  @see
  @return Result of the function call
 */
 WDI_Status
 WDI_ProcessTriggerBatchScanResultInd
 (
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
 );

#endif
/**
 @brief Process start OBSS scan request from Host

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHT40OBSSScanInd(
  WDI_ControlBlockType*  pWDICtx,  WDI_EventInfoType*   pEventData );


/**
 @brief Process stop OBSS scan request from Host

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHT40OBSSStopScanInd(
  WDI_ControlBlockType*  pWDICtx,  WDI_EventInfoType*   pEventData );

/*========================================================================
          Main DAL Control Path Response Processing API 
========================================================================*/


/**
 @brief Process Start Response function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStartRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Stop Response function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStopRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Close Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessCloseRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Shutdown Rsp function
        There is no shutdown response comming from HAL
        - function just kept for simmetry

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessShutdownRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Init Scan Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessInitScanRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Start Scan Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStartScanRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process End Scan Response function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEndScanRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Finish Scan Response function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFinishScanRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Join Response function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessJoinRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Config BSS Response function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigBSSRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Del BSS Response function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelBSSRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Post Assoc Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessPostAssocRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Del STA Key Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelSTARsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set BSS Key Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetBssKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Remove BSS Key Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveBssKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Set STA Key Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetStaKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Remove STA Key Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveStaKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Set STA Bcast Key Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetStaBcastKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Remove STA Bcast Key Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveStaBcastKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Add TSpec Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddTSpecRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Del TSpec Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelTSpecRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Update EDCA Parameters Rsp function (called when a  
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateEDCAParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Add BA Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddBASessionRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Del BA Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelBARsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef FEATURE_WLAN_ESE
/**
 @brief Process TSM stats Rsp function (called when a response
        is being received over the bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTsmStatsRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#endif


/**
 @brief Process Channel Switch Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessChannelSwitchRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Channel Switch Rsp function (called when a response
        is being received over the bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessChannelSwitchRsp_V1
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Config STA Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigStaRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Set Link State Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetLinkStateRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Update Channel Rsp function (called when a response is
        being received over the bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateChanRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Get Stats Rsp function (called when a response is   
        being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessGetStatsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Update Cfg Rsp function (called when a response is  
        being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateCfgRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Add BA Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddBARsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Add BA Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTriggerBARsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Update Beacon Params Rsp function (called when a response is  
        being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateBeaconParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Send Beacon template Rsp function (called when a response is  
        being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSendBeaconParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Update Probe Resp Template Rsp function (called 
        when a response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateProbeRspTemplateRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
  /**
 @brief Process Set Max Tx Power Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetMaxTxPowerRsp
( 
  WDI_ControlBlockType*          pWDICtx,
  WDI_EventInfoType*             pEventData
);

/**
 @brief Process Set Max Tx Power Per Band Rsp function (called when a response
        is being received over the bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetMaxTxPowerPerBandRsp
(
  WDI_ControlBlockType*          pWDICtx,
  WDI_EventInfoType*             pEventData
);

  /**
 @brief Process Set Tx Power Rsp function (called when a response
        is being received over the bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetTxPowerRsp
(
  WDI_ControlBlockType*          pWDICtx,
  WDI_EventInfoType*             pEventData
);

  /**
 @brief Process TDLS Link Establish Req Rsp function (called when a response
        is being received over the bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessLinkEstablishReqRsp
(
  WDI_ControlBlockType*          pWDICtx,
  WDI_EventInfoType*             pEventData
);


  /**
 @brief Process TDLS Chan Switch  Req Rsp function (called when a response
        is being received over the bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessChanSwitchReqRsp
(
  WDI_ControlBlockType*          pWDICtx,
  WDI_EventInfoType*             pEventData
);

/**
 @brief Process Nv download(called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessNvDownloadRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process P2P Group Owner Notice Of Absense Rsp function (called 
        when a response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessP2PGONOARsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Enter IMPS Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterImpsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Exit IMPS Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitImpsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Enter BMPS Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterBmpsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Exit BMPS Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitBmpsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Enter UAPSD Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterUapsdRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Exit UAPSD Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitUapsdRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process set UAPSD params Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetUapsdAcParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process update UAPSD params Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateUapsdParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Configure RXP filter Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigureRxpFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set beacon filter Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetBeaconFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process remove beacon filter Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemBeaconFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process set RSSI thresholds Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetRSSIThresoldsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process host offload Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHostOffloadRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Keep Alive Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessKeepAliveRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process wowl add ptrn Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlAddBcPtrnRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process wowl delete ptrn Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlDelBcPtrnRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process wowl enter Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlEnterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process wowl exit Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlExitRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Configure Apps CPU wakeup State Rsp function 
        (called when a response is being received over the bus
        from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigureAppsCpuWakeupStateRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
/**
 @brief Process Flush AC Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFlushAcRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process BT AMP event Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessBtAmpEventRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process ADD SELF STA Rsp function (called 
        when a response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddSTASelfRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

 /**
 @brief WDI_ProcessDelSTASelfRsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelSTASelfRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef FEATURE_OEM_DATA_SUPPORT
/**
 @brief Start Oem Data Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStartOemDataRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif

 /**
 @brief WDI_ProcessHostResumeRsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHostResumeRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process set tx per tracking Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetTxPerTrackingRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Power Params Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetPowerParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set TM Level Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetTmLevelRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/*==========================================================================
                        Indications from HAL
 ==========================================================================*/
/**
 @brief Process Low RSSI Indication function (called when an 
        indication of this kind is being received over the bus
        from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessLowRSSIInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Missed Beacon Indication function (called when 
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessMissedBeaconInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process Unk Addr Frame Indication function (called when 
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUnkAddrFrameInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/**
 @brief Process MIC Failure Indication function (called when an 
        indication of this kind is being received over the bus
        from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessMicFailureInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Fatal Failure Indication function (called when 
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFatalErrorInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Delete STA Indication function (called when 
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelSTAInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
*@brief Process Coex Indication function (called when
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessCoexInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
*@brief Process Tx Complete Indication function (called when
        an indication of this kind is being received over the
        bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTxCompleteInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
*@brief Process Tdls Indication function (called when
        an indication of this kind is being received over the
        bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTdlsInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
*@brief Process Noa Start Indication function (called when
        an indication of this kind is being received over the
        bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessP2pNoaStartInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
*@brief Process Noa Attr Indication function (called when
        an indication of this kind is being received over the
        bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessP2pNoaAttrInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
*@brief Process Tx Per Hit Indication function (called when
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTxPerHitInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef FEATURE_WLAN_LPHB
/**
 @brief WDI_ProcessLphbInd -
    This function will be invoked when FW detects low power
    heart beat failure

 @param  pWDICtx : wdi context
         pEventData : indication data

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessLphbInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif /* FEATURE_WLAN_LPHB */

/**
 @brief Process Periodic Tx Pattern Fw Indication function

 @param  pWDICtx:        pointer to the WLAN DAL context
         pEventData:     pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessPeriodicTxPtrnFwInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef WLAN_FEATURE_VOWIFI_11R
/**
 @brief Process Aggrgated Add TSpec Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAggrAddTSpecReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Add TSpec Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAggrAddTSpecRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#endif /* WLAN_FEATURE_VOWIFI_11R */

/**
 @brief WDI_ProcessFTMCommandReq
        Process FTM Command, simply route to HAL
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFTMCommandReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief WDI_ProcessFTMCommandRsp
        Process FTM Command Response from HAL, simply route to HDD FTM
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFTMCommandRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
/**
 @brief WDI_ProcessHALDumpCmdReq
        Process Hal Dump Command, simply route to HAL
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHALDumpCmdReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief WDI_ProcessHALDumpCmdRsp
        Process Hal Dump Command Response from HAL, simply route to HDD FTM
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHALDumpCmdRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief WDI_ProcessIbssPeerInactivityInd
        Process peer inactivity indication coming from HAL.

  @param  pWDICtx:         pointer to the WLAN DAL context
          pEventData:      pointer to the event information structure

  @see
  @return Result of the function call
*/
WDI_Status
WDI_ProcessIbssPeerInactivityInd

(
 WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);


/*========================================================================
         Internal Helper Routines 
========================================================================*/

/**
 @brief WDI_CleanCB - internal helper routine used to clean the 
        WDI Main Control Block
 
 @param pWDICtx - pointer to the control block

 @return Result of the function call
*/
WDI_Status
WDI_CleanCB
(
  WDI_ControlBlockType*  pWDICtx
);

/**
 @brief Main FSM Close function for all states except BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRequest
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Get message helper function - it allocates memory for a 
        message that is to be sent to HAL accross the bus and
        prefixes it with a send message header 
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         wdiReqType:      type of the request being sent
         uBufferLen:      message buffer len
         pMsgBuffer:      resulting allocated buffer
         puDataOffset:    offset in the buffer where the caller
         can start copying its message data
         puBufferSize:    the resulting buffer size (offset+buff
         len)
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_GetMessageBuffer
( 
  WDI_ControlBlockType*  pWDICtx, 
  WDI_RequestEnumType    wdiReqType, 
  wpt_uint16             usBufferLen,
  wpt_uint8**            pMsgBuffer, 
  wpt_uint16*            pusDataOffset, 
  wpt_uint16*            pusBufferSize
);

/**
 @brief WDI_DetectedDeviceError - called internally by DAL when 
        it has detected a failure in the device 
 
 @param  pWDICtx:        pointer to the WLAN DAL context 
         usErrorCode:    error code detected by WDI or received
                         from HAL
  
 @see
 @return None 
*/
void
WDI_DetectedDeviceError
(
  WDI_ControlBlockType*  pWDICtx,
  wpt_uint16             usErrorCode
);

/*=========================================================================
                   QUEUE SUPPORT UTILITY FUNCTIONS 
=========================================================================*/

/**
 @brief    Utility function used by the DAL Core to help queue a 
           request that cannot be processed right away. 
 @param 
    
    pWDICtx: - pointer to the WDI control block
    pEventData: - pointer to the evnt info that needs to be
    queued 
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_QueuePendingReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief    Utility function used by the DAL Core to clear any 
           pending requests - all req cb will be called with
           failure and the queue will be emptied.
 @param 
    
    pWDICtx: - pointer to the WDI control block
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_ClearPendingRequests
( 
  WDI_ControlBlockType*  pWDICtx
);

/**
 @brief    This callback is invoked by the wpt when a timer that 
           we started on send message has expire - this should
           never happen - it means device is stuck and cannot
           reply - trigger catastrophic failure 
 @param 
    
    pUserData: the callback data of the user (ptr to WDI CB)
    
 @see 
 @return None 
*/
void 
WDI_ResponseTimerCB
(
  void *pUserData
);

/*==========================================================================
                     CONTRL TRANSPORT INTERACTION
 
    Callback function registered with the control transport - for receiving
    notifications and packets 
==========================================================================*/
/**
 @brief    This callback is invoked by the control transport 
   when it wishes to send up a notification like the ones
   mentioned above.
 
 @param
    
    wctsHandle:       handle to the control transport service 
    wctsEvent:        the event being notified
    wctsNotifyCBData: the callback data of the user 
    
 @see  WCTS_OpenTransport
  
 @return None 
*/
void 
WDI_NotifyMsgCTSCB
(
  WCTS_HandleType        wctsHandle, 
  WCTS_NotifyEventType   wctsEvent,
  void*                  wctsNotifyCBData
);

/**
 @brief    This callback is invoked by the control transport 
           when it wishes to send up a packet received over the
           bus.
 
 @param
    
    wctsHandle:  handle to the control transport service 
    pMsg:        the packet
    uLen:        the packet length
    wctsRxMsgCBData: the callback data of the user 
    
 @see  WCTS_OpenTransport
  
 @return None 
*/
void 
WDI_RXMsgCTSCB 
(
  WCTS_HandleType       wctsHandle, 
  void*                 pMsg,
  wpt_uint32            uLen,
  void*                 wctsRxMsgCBData
);

/**
 @brief Process response helper function 

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessResponse
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Send message helper function - sends a message over the 
        bus using the control tranport and saves some info in
        the CB 
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pSendBuffer:     buffer to be sent
  
         uSendSize          size of the buffer to be sent
         pRspCb:            response callback - save in the WDI
         CB
         pUserData:         user data associated with the
         callback
         wdiExpectedResponse: the code of the response that is
         expected to be rx-ed for this request
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_SendMsg
( 
  WDI_ControlBlockType*  pWDICtx,  
  wpt_uint8*             pSendBuffer, 
  wpt_uint32             uSendSize, 
  void*                  pRspCb, 
  void*                  pUserData,
  WDI_ResponseEnumType   wdiExpectedResponse
);


/**
 @brief Send indication helper function - sends a message over 
        the bus using the control transport and saves some info
        in the CB
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pSendBuffer:     buffer to be sent
         usSendSize: size of the buffer to be sent
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_SendIndication
( 
  WDI_ControlBlockType*  pWDICtx,  
  wpt_uint8*             pSendBuffer, 
  wpt_uint32             usSendSize
);

/**
 @brief    Utility function used by the DAL Core to help dequeue
           and schedule for execution a pending request 
 @param 
    
    pWDICtx: - pointer to the WDI control block
    pEventData: - pointer to the evnt info that needs to be
    queued 
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_DequeuePendingReq
(
  WDI_ControlBlockType*  pWDICtx
);

/**
 @brief    Utility function used by the DAL Core to help queue 
           an association request that cannot be processed right
           away.- The assoc requests will be queued by BSSID 
 @param 
    
    pWDICtx: - pointer to the WDI control block
    pEventData: pointer to the evnt info that needs to be queued
    macBSSID: bssid
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_QueueNewAssocRequest
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData,
  wpt_macAddr            macBSSID
); 

/**
 @brief    Utility function used by the DAL Core to help queue 
           an association request that cannot be processed right
           away.- The assoc requests will be queued by BSSID 
 @param 
    
    pWDICtx: - pointer to the WDI control block
    pSession: - session in which to queue
    pEventData: pointer to the event info that needs to be
    queued
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_QueueAssocRequest
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_BSSSessionType*    pSession,
  WDI_EventInfoType*     pEventData
);

/**
 @brief    Utility function used by the DAL Core to help dequeue
           an association request that was pending
           The request will be queued up in front of the main
           pending queue for immediate processing
 @param 
    
    pWDICtx: - pointer to the WDI control block
  
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_DequeueAssocRequest
(
  WDI_ControlBlockType*  pWDICtx
);

/**
 @brief Helper routine used to init the BSS Sessions in the WDI control block 
  
 
 @param  pWDICtx:       pointer to the WLAN DAL context 
  
 @see
*/
void
WDI_ResetAssocSessions
( 
  WDI_ControlBlockType*   pWDICtx
);

/**
 @brief Helper routine used to find an empty session in the WDI 
        CB
  
 
 @param  pWDICtx:       pointer to the WLAN DAL context 
         pSession:      pointer to the session (if found) 
  
 @see
 @return Index of the session in the array 
*/
wpt_uint8
WDI_FindEmptySession
( 
  WDI_ControlBlockType*   pWDICtx,
  WDI_BSSSessionType**    ppSession
);

/**
 @brief Helper routine used to get the total count of active
        sessions


 @param  pWDICtx:       pointer to the WLAN DAL context
         macBSSID:      pointer to BSSID. If NULL, get all the session.
                        If not NULL, count ActiveSession by excluding (TRUE) or including (FALSE) skipBSSID.
         skipBSSID:     if TRUE, get all the sessions except matching to macBSSID. If FALSE, get all session.
                        This argument is ignored if macBSSID is NULL.
 @see
 @return Number of sessions in use
*/
wpt_uint8
WDI_GetActiveSessionsCount
(
  WDI_ControlBlockType*   pWDICtx,
  wpt_macAddr             macBSSID,
  wpt_boolean             skipBSSID
);

/**
 @brief Helper routine used to delete session in the WDI 
        CB
  
 
 @param  pWDICtx:       pointer to the WLAN DAL context 
         pSession:      pointer to the session (if found) 
  
 @see
 @return Index of the session in the array 
*/
void 
WDI_DeleteSession
( 
  WDI_ControlBlockType*   pWDICtx,
  WDI_BSSSessionType*     ppSession
);

/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pWDICtx:   pointer to the WLAN DAL context 
         macBSSID:  BSSID of the session
         ppSession: out pointer to the session (if found)
  
 @see
 @return Index of the session in the array 
*/
wpt_uint8
WDI_FindAssocSession
( 
  WDI_ControlBlockType*   pWDICtx,
  wpt_macAddr             macBSSID,
  WDI_BSSSessionType**    ppSession
);


/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pWDICtx:   pointer to the WLAN DAL context 
         usBssIdx:  BSS Index of the session
         ppSession: out pointer to the session (if found)
  
 @see
 @return Index of the session in the array 
*/
wpt_uint8
WDI_FindAssocSessionByBSSIdx
( 
  WDI_ControlBlockType*   pWDICtx,
  wpt_uint16              usBssIdx,
  WDI_BSSSessionType**    ppSession
);

/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pWDICtx:   pointer to the WLAN DAL context 
         usBssIdx:  BSS Index of the session
         ppSession: out pointer to the session (if found)
  
 @see
 @return Index of the session in the array 
*/
wpt_uint8
WDI_FindAssocSessionByIdx
( 
  WDI_ControlBlockType*   pWDICtx,
  wpt_uint16              usBssIdx,
  WDI_BSSSessionType**    ppSession
);

/**
 @brief Helper routine used to find a session based on the BSSID 
 @param  pContext:   pointer to the WLAN DAL context 
 @param  pDPContext:   pointer to the Datapath context 
  
 @see
 @return 
*/
void
WDI_DS_AssignDatapathContext 
(
  void *pContext, 
  void *pDPContext
);

/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pContext:   pointer to the WLAN DAL context 
  
 @see
 @return pointer to Datapath context
*/
void *
WDI_DS_GetDatapathContext 
(
  void *pContext
);

/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pContext:   pointer to the WLAN DAL context 
 @param  pDTDriverContext:   pointer to the Transport Driver context 
  
 @see
 @return void
*/
void
WDT_AssignTransportDriverContext 
(
  void *pContext, 
  void *pDTDriverContext
);

/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pWDICtx:   pointer to the WLAN DAL context 
  
 @see
 @return pointer to datapath context 
*/
void *
WDT_GetTransportDriverContext 
(
  void *pContext
);

#ifdef FEATURE_WLAN_SCAN_PNO
/**
 @brief Process Set Preferred Network List Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetPreferredNetworkReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Set RSSI Filter Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetRssiFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Update Scan Params function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateScanParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Preferred Network Found Indication function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessPrefNetworkFoundInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process PNO Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetPreferredNetworkRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process RSSI Filter Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetRssiFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Update Scan Params Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateScanParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif // FEATURE_WLAN_SCAN_PNO


#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
/**
 @brief Process Start Roam Candidate Lookup Request function

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRoamScanOffloadReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
/**
 @brief Process Start Roam Candidate Lookup Response function (called when a
        response is being received over the bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRoamScanOffloadRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif


#ifdef WLAN_FEATURE_PACKET_FILTERING
/**
 @brief Process 8023 Multicast List Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_Process8023MulticastListReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Receive Filter Set Filter Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessReceiveFilterSetFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process D0 PC Filter Match Count Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFilterMatchCountReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Receive Filter Clear Filter Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessReceiveFilterClearFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process 8023 Multicast List Response function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_Process8023MulticastListRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Receive Filter Set Filter Response function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessReceiveFilterSetFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process D0 PC Filter Match Count Response function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFilterMatchCountRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Receive Filter Clear Filter Response function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessReceiveFilterClearFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif // WLAN_FEATURE_PACKET_FILTERING

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/**
 @brief Process set GTK Offload Request function 
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessGTKOffloadReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process GTK Offload Get Information Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessGTKOffloadGetInfoReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process host offload Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessGtkOffloadRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process GTK Offload Get Information Response function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessGTKOffloadGetInfoRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif // WLAN_FEATURE_GTK_OFFLOAD

#ifdef WLAN_WAKEUP_EVENTS
WDI_Status
WDI_ProcessWakeReasonInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif // WLAN_WAKEUP_EVENTS

/**
 @brief Process Host-FW Capability Exchange Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFeatureCapsExchangeReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process Host-FW Capability Exchange Response function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFeatureCapsExchangeRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef WLAN_FEATURE_11AC
WDI_Status
WDI_ProcessUpdateVHTOpModeReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessUpdateVHTOpModeRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
/**
 *  @brief WDI_wdiEdTypeEncToEdTypeEnc -
 *  The firmware expects the Encryption type to be in EdType.
 *  This function converts the WdiEdType encryption to EdType.
 *  @param tEdType    : EdType to which the encryption needs to be converted.
 *  @param WDI_EdType : wdiEdType passed from the upper layer.
 *  @see
 *  @return none
 *  */
void
WDI_wdiEdTypeEncToEdTypeEnc
(
 tEdType *EdType,
 WDI_EdType wdiEdType
);
#endif

#ifdef FEATURE_WLAN_LPHB
/**
 @brief WDI_ProcessLphbCfgRsp -
    LPHB configuration response from FW

 @param  pWDICtx : wdi context
         pEventData : indication data

 @see
 @return Result of the function call
*/
WDI_Status WDI_ProcessLphbCfgRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif /* FEATURE_WLAN_LPHB */

/**
 @brief Process Rate Update Indication and post it to HAL

 @param  pWDICtx:    pointer to the WLAN DAL context
         pEventData: pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRateUpdateInd
(
    WDI_ControlBlockType*  pWDICtx,
    WDI_EventInfoType*     pEventData
);

#ifdef FEATURE_WLAN_BATCH_SCAN
/**
 @brief WDI_ProcessSetBatchScanRsp -
     Process set batch scan response from FW

 @param  pWDICtx : wdi context
         pEventData : indication data

 @see
 @return Result of the function call
*/
WDI_Status WDI_ProcessSetBatchScanRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessGetBcnMissRateReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessGetBcnMissRateRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

/**
 @brief Process batch scan response from FW

 @param  pWDICtx:        pointer to the WLAN DAL context
         pEventData:     pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessBatchScanResultInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#endif /* FEATURE_WLAN_BATCH_SCAN */

WDI_Status
WDI_ProcessGetFwStatsReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessGetFwStatsRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef FEATURE_WLAN_CH_AVOID
/**
 @brief v -


 @param  pWDICtx : wdi context
         pEventData : indication data
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessChAvoidInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif /* FEATURE_WLAN_CH_AVOID */

/**
 @brief v -


 @param  pWDICtx : wdi context
         pEventData : indication data
 @see
 @return Result of the function call
*/
WDI_Status
WDI_printRegInfo
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

#ifdef WLAN_FEATURE_EXTSCAN
WDI_Status
WDI_ProcessEXTScanStartReq
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);
WDI_Status
WDI_ProcessEXTScanStopReq
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);
WDI_Status
WDI_ProcessEXTScanStartRsp
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);
WDI_Status
WDI_ProcessEXTScanStopRsp
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanGetCachedResultsReq
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);
WDI_Status
WDI_ProcessEXTScanGetCachedResultsRsp
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanProgressInd
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanGetCapabilitiesReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanGetCapabilitiesRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanSetBSSIDHotlistReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanSetHotlistBSSIDRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanResetBSSIDHotlistReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanResetHotlistBSSIDRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanSetSignifRSSIChangeReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanSetSignfRSSIChangeRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanResetSignfRSSIChangeReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanResetSignfRSSIChangeRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanScanAvailableInd
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanResultInd
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanBssidHotListResultInd
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEXTScanSignfRssiResultInd
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

#endif /* WLAN_FEATURE_EXTSCAN */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
WDI_Status
WDI_ProcessLLStatsSetRsp
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessLLStatsSetReq
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessLLStatsGetRsp
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessLLStatsGetReq
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessLLStatsClearRsp
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessLLStatsClearReq
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessLinkLayerStatsResultsInd
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

WDI_Status
WDI_delBaInd
(
   WDI_ControlBlockType*  pWDICtx,
   WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessSpoofMacAddrReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
WDI_Status
WDI_ProcessSpoofMacAddrRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEncryptMsgReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

WDI_Status
WDI_ProcessEncryptMsgRsp
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);
#endif /*WLAN_QCT_WDI_I_H*/

