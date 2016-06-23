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

#ifndef __WEXT_IW_H__
#define __WEXT_IW_H__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/timer.h>
#include "vos_event.h"

/*
 * order of parameters in addTs private ioctl
 */
#define HDD_WLAN_WMM_PARAM_HANDLE                       0
#define HDD_WLAN_WMM_PARAM_TID                          1
#define HDD_WLAN_WMM_PARAM_DIRECTION                    2
#define HDD_WLAN_WMM_PARAM_APSD                         3
#define HDD_WLAN_WMM_PARAM_USER_PRIORITY                4
#define HDD_WLAN_WMM_PARAM_NOMINAL_MSDU_SIZE            5
#define HDD_WLAN_WMM_PARAM_MAXIMUM_MSDU_SIZE            6
#define HDD_WLAN_WMM_PARAM_MINIMUM_DATA_RATE            7
#define HDD_WLAN_WMM_PARAM_MEAN_DATA_RATE               8
#define HDD_WLAN_WMM_PARAM_PEAK_DATA_RATE               9
#define HDD_WLAN_WMM_PARAM_MAX_BURST_SIZE              10
#define HDD_WLAN_WMM_PARAM_MINIMUM_PHY_RATE            11
#define HDD_WLAN_WMM_PARAM_SURPLUS_BANDWIDTH_ALLOWANCE 12
#define HDD_WLAN_WMM_PARAM_SERVICE_INTERVAL            13
#define HDD_WLAN_WMM_PARAM_SUSPENSION_INTERVAL         14
#define HDD_WLAN_WMM_PARAM_BURST_SIZE_DEFN             15
#define HDD_WLAN_WMM_PARAM_ACK_POLICY                  16
#define HDD_WLAN_WMM_PARAM_INACTIVITY_INTERVAL         17
#define HDD_WLAN_WMM_PARAM_MAX_SERVICE_INTERVAL        18
#define HDD_WLAN_WMM_PARAM_COUNT                       19

#define MHZ 6

#define WE_MAX_STR_LEN                                 1024
#define WLAN_HDD_UI_BAND_AUTO                          0
#define WLAN_HDD_UI_BAND_5_GHZ                         1
#define WLAN_HDD_UI_BAND_2_4_GHZ                       2
/* SETBAND x */
/* 012345678 */
#define WLAN_HDD_UI_SET_BAND_VALUE_OFFSET              8

#ifdef WLAN_SOFTAP_VSTA_FEATURE
/* 41 - STA support with Virtual STA feature
 * 1  - Broadcast STA
 * 4  - reserved Self STA: ap_self_sta, wlan_self_sta,
 *      wlan_peer_sta, p2p_self_sta
 * 4  - General Purpose Stations to support Virtual STAs
 */
#define VSTA_NUM_STA           41
#define VSTA_NUM_RESV_SELFSTA  4
#define VSTA_NUM_BC_STA        1
#define VSTA_NUM_GPSTA         4
#define VSTA_NUM_ASSOC_STA     (VSTA_NUM_STA - VSTA_NUM_RESV_SELFSTA -\
                                VSTA_NUM_BC_STA - VSTA_NUM_GPSTA )
#endif

/* 12 - STA support without Virtual STA feature
 * 1  - Broadcast STA
 * 1  - reserved Self STA: ap_self_sta or wlan_self_sta,
 */
#define NUM_STA           12
#define NUM_RESV_SELFSTA  1
#define NUM_BC_STA        1
#define NUM_ASSOC_STA     (NUM_STA - NUM_RESV_SELFSTA - NUM_BC_STA)

typedef enum
{
   HDD_WLAN_WMM_DIRECTION_UPSTREAM      = 0,
   HDD_WLAN_WMM_DIRECTION_DOWNSTREAM    = 1,
   HDD_WLAN_WMM_DIRECTION_BIDIRECTIONAL = 2,
} hdd_wlan_wmm_direction_e;

typedef enum
{
   HDD_WLAN_WMM_POWER_SAVE_LEGACY       = 0,
   HDD_WLAN_WMM_POWER_SAVE_UAPSD        = 1,
} hdd_wlan_wmm_power_save_e;

typedef enum
{
   // TSPEC/re-assoc done, async
   HDD_WLAN_WMM_STATUS_SETUP_SUCCESS = 0,
   // no need to setup TSPEC since ACM=0 and no UAPSD desired, sync + async
   HDD_WLAN_WMM_STATUS_SETUP_SUCCESS_NO_ACM_NO_UAPSD = 1,
   // no need to setup TSPEC since ACM=0 and UAPSD already exists, sync + async
   HDD_WLAN_WMM_STATUS_SETUP_SUCCESS_NO_ACM_UAPSD_EXISTING = 2,
   // TSPEC result pending, sync
   HDD_WLAN_WMM_STATUS_SETUP_PENDING = 3,
   // TSPEC/re-assoc failed, sync + async
   HDD_WLAN_WMM_STATUS_SETUP_FAILED = 4,
   // Request rejected due to invalid params, sync + async
   HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM = 5,
   // TSPEC request rejected since AP!=QAP, sync
   HDD_WLAN_WMM_STATUS_SETUP_FAILED_NO_WMM = 6,

   // TSPEC modification/re-assoc successful, async
   HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS = 7,
   // TSPEC modification a no-op since ACM=0 and no change in UAPSD, sync + async
   HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS_NO_ACM_NO_UAPSD = 8,
   // TSPEC modification a no-op since ACM=0 and requested U-APSD already exists, sync + async
   HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS_NO_ACM_UAPSD_EXISTING = 9,
   // TSPEC result pending, sync
   HDD_WLAN_WMM_STATUS_MODIFY_PENDING = 10,
   // TSPEC modification failed, prev TSPEC in effect, sync + async
   HDD_WLAN_WMM_STATUS_MODIFY_FAILED = 11,
   // TSPEC modification request rejected due to invalid params, sync + async
   HDD_WLAN_WMM_STATUS_MODIFY_FAILED_BAD_PARAM = 12,

   // TSPEC release successful, sync and also async
   HDD_WLAN_WMM_STATUS_RELEASE_SUCCESS = 13,
   // TSPEC release pending, sync
   HDD_WLAN_WMM_STATUS_RELEASE_PENDING = 14,
   // TSPEC release failed, sync + async
   HDD_WLAN_WMM_STATUS_RELEASE_FAILED = 15,
   // TSPEC release rejected due to invalid params, sync
   HDD_WLAN_WMM_STATUS_RELEASE_FAILED_BAD_PARAM = 16,
   // TSPEC modified due to the mux'ing of requests on ACs, async

   HDD_WLAN_WMM_STATUS_MODIFIED = 17,
   // TSPEC revoked by AP, async
   HDD_WLAN_WMM_STATUS_LOST = 18,
   // some internal failure like memory allocation failure, etc, sync
   HDD_WLAN_WMM_STATUS_INTERNAL_FAILURE = 19, 

   // U-APSD failed during setup but OTA setup (whether TSPEC exchnage or
   // re-assoc) was done so app should release this QoS, async
   HDD_WLAN_WMM_STATUS_SETUP_UAPSD_SET_FAILED = 20,
   // U-APSD failed during modify, but OTA setup (whether TSPEC exchnage or
   // re-assoc) was done so app should release this QoS, async
   HDD_WLAN_WMM_STATUS_MODIFY_UAPSD_SET_FAILED = 21

} hdd_wlan_wmm_status_e;

/** TS Info Ack Policy */
typedef enum
{
   HDD_WLAN_WMM_TS_INFO_ACK_POLICY_NORMAL_ACK      = 0,
   HDD_WLAN_WMM_TS_INFO_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK    = 1,
} hdd_wlan_wmm_ts_info_ack_policy_e;

/** vendor element ID */
#define IE_EID_VENDOR        ( 221 ) /* 0xDD */
#define IE_LEN_SIZE          1
#define IE_EID_SIZE          1
#define IE_VENDOR_OUI_SIZE   4

/** Maximum Length of WPA/RSN IE */
#define MAX_WPA_RSN_IE_LEN 40

/** Maximum Number of WEP KEYS */
#define MAX_WEP_KEYS 4

/** Ether Address Length */
#define ETHER_ADDR_LEN 6

/** Enable 11d */
#define ENABLE_11D  1

/** Disable 11d */
#define DISABLE_11D 0

/* 
   refer wpa.h in wpa supplicant code for REASON_MICHAEL_MIC_FAILURE

   supplicant sets REASON_MICHAEL_MIC_FAILURE as the reason code when it sends the MLME deauth IOCTL 
   for TKIP counter measures
*/
#define HDD_REASON_MICHAEL_MIC_FAILURE 14

/* 
  * These are for TLV fields in WPS IE
  */
#define HDD_WPS_UUID_LEN                    16 
#define HDD_WPS_ELEM_VERSION                0x104a 
#define HDD_WPS_ELEM_REQUEST_TYPE           0x103a 
#define HDD_WPS_ELEM_CONFIG_METHODS         0x1008 
#define HDD_WPS_ELEM_UUID_E                 0x1047 
#define HDD_WPS_ELEM_PRIMARY_DEVICE_TYPE    0x1054 
#define HDD_WPS_ELEM_RF_BANDS               0x103c 
#define HDD_WPS_ELEM_ASSOCIATION_STATE      0x1002 
#define HDD_WPS_ELEM_CONFIGURATION_ERROR    0x1009
#define HDD_WPS_ELEM_DEVICE_PASSWORD_ID     0x1012 

#define HDD_WPA_ELEM_VENDOR_EXTENSION       0x1049

#define HDD_WPS_MANUFACTURER_LEN            64
#define HDD_WPS_MODEL_NAME_LEN              32
#define HDD_WPS_MODEL_NUM_LEN               32
#define HDD_WPS_SERIAL_NUM_LEN              32
#define HDD_WPS_DEVICE_OUI_LEN               4
#define HDD_WPS_DEVICE_NAME_LEN             32

#define HDD_WPS_ELEM_WPS_STATE              0x1044
#define HDD_WPS_ELEM_APSETUPLOCK            0x1057
#define HDD_WPS_ELEM_SELECTEDREGISTRA       0x1041  
#define HDD_WPS_ELEM_RSP_TYPE               0x103B
#define HDD_WPS_ELEM_MANUFACTURER           0x1021
#define HDD_WPS_ELEM_MODEL_NAME             0x1023
#define HDD_WPS_ELEM_MODEL_NUM              0x1024
#define HDD_WPS_ELEM_SERIAL_NUM             0x1042 
#define HDD_WPS_ELEM_DEVICE_NAME            0x1011
#define HDD_WPS_ELEM_REGISTRA_CONF_METHODS  0x1053



#define WPS_OUI_TYPE   "\x00\x50\xf2\x04"
#define WPS_OUI_TYPE_SIZE  4

#define SS_OUI_TYPE    "\x00\x16\x32"
#define SS_OUI_TYPE_SIZE   3

#define P2P_OUI_TYPE   "\x50\x6f\x9a\x09"
#define P2P_OUI_TYPE_SIZE  4

#define HS20_OUI_TYPE   "\x50\x6f\x9a\x10"
#define HS20_OUI_TYPE_SIZE  4

#define OSEN_OUI_TYPE   "\x50\x6f\x9a\x12"
#define OSEN_OUI_TYPE_SIZE  4

#ifdef WLAN_FEATURE_WFD
#define WFD_OUI_TYPE   "\x50\x6f\x9a\x0a"
#define WFD_OUI_TYPE_SIZE  4
#endif

typedef enum
{
    eWEXT_WPS_OFF = 0,
    eWEXT_WPS_ON = 1,
}hdd_wps_mode_e;

typedef enum
{
    DRIVER_POWER_MODE_AUTO = 0,
    DRIVER_POWER_MODE_ACTIVE = 1,
} hdd_power_mode_e;

typedef enum
{
    WEXT_SCAN_PENDING_GIVEUP = 0,
    WEXT_SCAN_PENDING_PIGGYBACK = 1,
    WEXT_SCAN_PENDING_DELAY = 2,
    WEXT_SCAN_PENDING_MAX
} hdd_scan_pending_option_e;

/* 
 * This structure contains the interface level (granularity) 
 * configuration information in support of wireless extensions. 
 */
typedef struct hdd_wext_state_s 
{
   /** The CSR "desired" Profile */
   tCsrRoamProfile roamProfile; 

   /** BSSID to which connect request is received */
   tCsrBssid req_bssId;

   /** The association status code */ 
   v_U32_t statusCode;

   /** wpa version WPA/WPA2/None*/
   v_S31_t wpaVersion; 
   
   /**WPA or RSN IE*/
   u_int8_t WPARSNIE[MAX_WPA_RSN_IE_LEN]; 

   /**gen IE */
   tSirAddie genIE;

   /**Additional IE for assoc */
   tSirAddie assocAddIE; 
   
   /**auth key mgmt */
   v_S31_t authKeyMgmt; 

    /**vos event */
   vos_event_t  vosevent;

   vos_event_t  scanevent;

   /**Counter measure state, Started/Stopped*/
   v_BOOL_t mTKIPCounterMeasures;  

   /**Completion Variable*/
   struct completion completion_var;

#ifdef FEATURE_OEM_DATA_SUPPORT
   /* oem data req in Progress */
   v_BOOL_t oemDataReqInProgress;

   /* oem data req ID */
   v_U32_t oemDataReqID;
#endif

#ifdef FEATURE_WLAN_ESE
   /* ESE state variables */
   v_BOOL_t isESEConnection;
   eCsrAuthType collectedAuthType; /* Collected from ALL SIOCSIWAUTH Ioctls. Will be negotiatedAuthType - in tCsrProfile */
#endif
}hdd_wext_state_t;

typedef struct ccp_freq_chan_map_s{
    // List of frequencies
    v_U32_t freq;
    v_U32_t chan;
}hdd_freq_chan_map_t;

#define wlan_hdd_get_wps_ie_ptr(ie, ie_len) \
    wlan_hdd_get_vendor_oui_ie_ptr(WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE, ie, ie_len)

#define wlan_hdd_get_p2p_ie_ptr(ie, ie_len) \
    wlan_hdd_get_vendor_oui_ie_ptr(P2P_OUI_TYPE, P2P_OUI_TYPE_SIZE, ie, ie_len)

#ifdef WLAN_FEATURE_WFD
#define wlan_hdd_get_wfd_ie_ptr(ie, ie_len) \
    wlan_hdd_get_vendor_oui_ie_ptr(WFD_OUI_TYPE, WFD_OUI_TYPE_SIZE, ie, ie_len)
#endif

extern int hdd_UnregisterWext(struct net_device *dev);
extern int hdd_register_wext(struct net_device *dev);
extern int hdd_wlan_get_freq(v_U32_t chan,v_U32_t *freq);
extern int hdd_wlan_get_rts_threshold(hdd_adapter_t *pAdapter,
                                      union iwreq_data *wrqu);
extern int hdd_wlan_get_frag_threshold(hdd_adapter_t *pAdapter,
                                      union iwreq_data *wrqu);
extern void hdd_wlan_get_version(hdd_adapter_t *pAdapter,
                                 union iwreq_data *wrqu, char *extra);

extern int iw_get_scan(struct net_device *dev, 
                       struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra);

extern int iw_set_scan(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra);

extern int iw_set_cscan(struct net_device *dev, struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra);

extern int iw_set_essid(struct net_device *dev, 
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra);

extern int iw_get_essid(struct net_device *dev, 
                       struct iw_request_info *info,
                       struct iw_point *dwrq, char *extra);


extern int iw_set_ap_address(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra);

extern int iw_get_ap_address(struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu, char *extra);

extern int iw_set_auth(struct net_device *dev,struct iw_request_info *info,
                       union iwreq_data *wrqu,char *extra);

extern int iw_get_auth(struct net_device *dev,struct iw_request_info *info,
                       union iwreq_data *wrqu,char *extra);

VOS_STATUS iw_set_pno(struct net_device *dev, struct iw_request_info *info,
                      union iwreq_data *wrqu, char *extra, int nOffset);


VOS_STATUS iw_set_rssi_filter(struct net_device *dev, struct iw_request_info *info,
                              union iwreq_data *wrqu, char *extra, int nOffset);

VOS_STATUS iw_set_power_params(struct net_device *dev, struct iw_request_info *info,
                      union iwreq_data *wrqu, char *extra, int nOffset);

void ccmCfgSetCallback(tHalHandle halHandle, tANI_S32 result);

extern int iw_set_var_ints_getnone(struct net_device *dev, struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra);

extern int iw_set_three_ints_getnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra);

extern int hdd_priv_get_data(struct iw_point *p_priv_data,
                             union iwreq_data *wrqu);

extern void *mem_alloc_copy_from_user_helper(const void *wrqu_data, size_t len);

void hdd_clearRoamProfileIe( hdd_adapter_t *pAdapter);
void hdd_GetClassA_statisticsCB(void *pStats, void *pContext);

VOS_STATUS wlan_hdd_check_ula_done(hdd_adapter_t *pAdapter);

v_U8_t* wlan_hdd_get_vendor_oui_ie_ptr(v_U8_t *oui, v_U8_t oui_size, 
                       v_U8_t *ie, int ie_len);

VOS_STATUS wlan_hdd_enter_bmps(hdd_adapter_t *pAdapter, int mode);

VOS_STATUS wlan_hdd_exit_lowpower(hdd_context_t *pHddCtx,
                                       hdd_adapter_t *pAdapter);

VOS_STATUS wlan_hdd_enter_lowpower(hdd_context_t *pHddCtx);

VOS_STATUS wlan_hdd_get_classAstats(hdd_adapter_t *pAdapter);

VOS_STATUS wlan_hdd_get_station_stats(hdd_adapter_t *pAdapter);

VOS_STATUS wlan_hdd_get_rssi(hdd_adapter_t *pAdapter, v_S7_t *rssi_value);

VOS_STATUS wlan_hdd_get_snr(hdd_adapter_t *pAdapter, v_S7_t *snr);

void hdd_wmm_tx_snapshot(hdd_adapter_t *pAdapter);

#ifdef FEATURE_WLAN_TDLS
VOS_STATUS iw_set_tdls_params(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra, int nOffset);
int iw_set_tdlsoffchannelmode(hdd_adapter_t *pAdapter, int offchanmode);
int iw_set_tdlssecoffchanneloffset(hdd_context_t *pHddCtx, int offchanoffset);
int iw_set_tdlsoffchannel(hdd_context_t *pHddCtx, int offchannel);
#endif
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
VOS_STATUS wlan_hdd_get_roam_rssi(hdd_adapter_t *pAdapter, v_S7_t *rssi_value);
#endif

#ifdef WLAN_FEATURE_PACKET_FILTERING
void wlan_hdd_set_mc_addr_list(hdd_adapter_t *pAdapter, v_U8_t set);
#endif
void* wlan_hdd_change_country_code_callback(void *pAdapter);

int hdd_setBand(struct net_device *dev, u8 ui_band);
int hdd_setBand_helper(struct net_device *dev, const char *command);

#endif // __WEXT_IW_H__

