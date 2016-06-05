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




/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *
    \file csrApi.h

    Exports and types for the Common Scan and Roaming Module interfaces.
   ========================================================================== */
#ifndef CSRAPI_H__
#define CSRAPI_H__

#include "sirApi.h"
#include "sirMacProtDef.h"
#include "csrLinkList.h"
typedef enum
{
    eCSR_AUTH_TYPE_NONE,    //never used
    // MAC layer authentication types
    eCSR_AUTH_TYPE_OPEN_SYSTEM,
    eCSR_AUTH_TYPE_SHARED_KEY,
    eCSR_AUTH_TYPE_AUTOSWITCH,

    // Upper layer authentication types
    eCSR_AUTH_TYPE_WPA,
    eCSR_AUTH_TYPE_WPA_PSK,
    eCSR_AUTH_TYPE_WPA_NONE,

    eCSR_AUTH_TYPE_RSN,
    eCSR_AUTH_TYPE_RSN_PSK,
#if defined WLAN_FEATURE_VOWIFI_11R
    eCSR_AUTH_TYPE_FT_RSN,
    eCSR_AUTH_TYPE_FT_RSN_PSK,
#endif
#ifdef FEATURE_WLAN_WAPI
    eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE,
    eCSR_AUTH_TYPE_WAPI_WAI_PSK,
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_ESE
    eCSR_AUTH_TYPE_CCKM_WPA,
    eCSR_AUTH_TYPE_CCKM_RSN,
#endif /* FEATURE_WLAN_ESE */
#ifdef WLAN_FEATURE_11W
    eCSR_AUTH_TYPE_RSN_PSK_SHA256,
    eCSR_AUTH_TYPE_RSN_8021X_SHA256,
#endif
    eCSR_NUM_OF_SUPPORT_AUTH_TYPE,
    eCSR_AUTH_TYPE_FAILED = 0xff,
    eCSR_AUTH_TYPE_UNKNOWN = eCSR_AUTH_TYPE_FAILED,

}eCsrAuthType;


typedef enum
{
    eCSR_ENCRYPT_TYPE_NONE,
    eCSR_ENCRYPT_TYPE_WEP40_STATICKEY,
    eCSR_ENCRYPT_TYPE_WEP104_STATICKEY,

    eCSR_ENCRYPT_TYPE_WEP40,
    eCSR_ENCRYPT_TYPE_WEP104,
    eCSR_ENCRYPT_TYPE_TKIP,
    eCSR_ENCRYPT_TYPE_AES,
#ifdef FEATURE_WLAN_WAPI
    eCSR_ENCRYPT_TYPE_WPI, //WAPI
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_ESE
    eCSR_ENCRYPT_TYPE_KRK,
#endif /* FEATURE_WLAN_ESE */
#ifdef WLAN_FEATURE_11W
    //11w BIP
    eCSR_ENCRYPT_TYPE_AES_CMAC,
#endif
    eCSR_ENCRYPT_TYPE_ANY,
    eCSR_NUM_OF_ENCRYPT_TYPE = eCSR_ENCRYPT_TYPE_ANY,

    eCSR_ENCRYPT_TYPE_FAILED = 0xff,
    eCSR_ENCRYPT_TYPE_UNKNOWN = eCSR_ENCRYPT_TYPE_FAILED,

}eCsrEncryptionType;

/*---------------------------------------------------------------------------
   Enumeration of the various Security types
---------------------------------------------------------------------------*/
typedef enum
{
    eCSR_SECURITY_TYPE_WPA,
    eCSR_SECURITY_TYPE_RSN,
#ifdef FEATURE_WLAN_WAPI
    eCSR_SECURITY_TYPE_WAPI,
#endif /* FEATURE_WLAN_WAPI */
    eCSR_SECURITY_TYPE_UNKNOWN,

}eCsrSecurityType;

typedef enum
{
    eCSR_DOT11_MODE_TAURUS = 0, //This mean everything because it covers all thing we support
    eCSR_DOT11_MODE_abg = 0x0001,    //11a/b/g only, no HT, no proprietary
    eCSR_DOT11_MODE_11a = 0x0002,
    eCSR_DOT11_MODE_11b = 0x0004,
    eCSR_DOT11_MODE_11g = 0x0008,
    eCSR_DOT11_MODE_11n = 0x0010,
    eCSR_DOT11_MODE_POLARIS = 0x0020,
    eCSR_DOT11_MODE_TITAN = 0x0040,
    eCSR_DOT11_MODE_11g_ONLY = 0x0080,
    eCSR_DOT11_MODE_11n_ONLY = 0x0100,
    eCSR_DOT11_MODE_TAURUS_ONLY = 0x0200,
    eCSR_DOT11_MODE_11b_ONLY = 0x0400,
    eCSR_DOT11_MODE_11a_ONLY = 0x0800,
#ifdef WLAN_FEATURE_11AC
    eCSR_DOT11_MODE_11ac     = 0x1000,
    eCSR_DOT11_MODE_11ac_ONLY = 0x2000,
#endif
    //This is for WIFI test. It is same as eWNIAPI_MAC_PROTOCOL_ALL except when it starts IBSS in 11B of 2.4GHz
    //It is for CSR internal use
    eCSR_DOT11_MODE_AUTO = 0x4000,

    eCSR_NUM_PHY_MODE = 16,     //specify the number of maximum bits for phyMode
}eCsrPhyMode;


typedef tANI_U8 tCsrBssid[WNI_CFG_BSSID_LEN];

typedef enum
{
    eCSR_BSS_TYPE_NONE,
    eCSR_BSS_TYPE_INFRASTRUCTURE,
    eCSR_BSS_TYPE_INFRA_AP,       // SoftAP AP
    eCSR_BSS_TYPE_IBSS,           // an IBSS network we will NOT start
    eCSR_BSS_TYPE_START_IBSS,     // an IBSS network we will start if no partners detected.
    eCSR_BSS_TYPE_WDS_AP,         // BT-AMP AP
    eCSR_BSS_TYPE_WDS_STA,        // BT-AMP station
    eCSR_BSS_TYPE_ANY,            // any BSS type (IBSS or Infrastructure).
}eCsrRoamBssType;



typedef enum {
    eCSR_SCAN_REQUEST_11D_SCAN = 1,
    eCSR_SCAN_REQUEST_FULL_SCAN,
    eCSR_SCAN_IDLE_MODE_SCAN,
    eCSR_SCAN_HO_BG_SCAN, // bg scan request in NRT & RT Handoff sub-states
    eCSR_SCAN_HO_PROBE_SCAN, // directed probe on an entry from the candidate list
    eCSR_SCAN_HO_NT_BG_SCAN, // bg scan request in NT  sub-state
    eCSR_SCAN_P2P_DISCOVERY,

    eCSR_SCAN_SOFTAP_CHANNEL_RANGE,
    eCSR_SCAN_P2P_FIND_PEER,
}eCsrRequestType;
typedef enum {
    eCSR_SCAN_RESULT_GET = 0,
    eCSR_SCAN_RESULT_FLUSH = 1,     //to delete all cached scan results
}eCsrScanResultCmd;

typedef enum
{
    eCSR_SCAN_SUCCESS,
    eCSR_SCAN_FAILURE,
    eCSR_SCAN_ABORT,
   eCSR_SCAN_FOUND_PEER,
}eCsrScanStatus;

/* Reason to abort the scan
 * The reason can used later to decide whether to update the scan results
 * to upper layer or not
 */
typedef enum
{
    eCSR_SCAN_ABORT_DEFAULT = 1,
    eCSR_SCAN_ABORT_DUE_TO_BAND_CHANGE, //Scan aborted due to band change
}eCsrAbortReason;

typedef enum
{
   eCSR_INI_SINGLE_CHANNEL_CENTERED = 0,
   eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY,
   eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY,
#ifdef WLAN_FEATURE_11AC
   eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED,
   eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED,
   eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED,
   eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW,
   eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW,
   eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH,
   eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH,
#endif
   eCSR_INI_CHANNEL_BONDING_STATE_MAX
}eIniChanBondState;

#define CSR_SCAN_TIME_DEFAULT       0
#define CSR_VALUE_IGNORED           0xFFFFFFFF
#define CSR_RSN_PMKID_SIZE          16
#define CSR_MAX_PMKID_ALLOWED       32
#define CSR_WEP40_KEY_LEN       5
#define CSR_WEP104_KEY_LEN      13
#define CSR_TKIP_KEY_LEN        32
#define CSR_AES_KEY_LEN         16
#define CSR_MAX_TX_POWER        ( WNI_CFG_CURRENT_TX_POWER_LEVEL_STAMAX )
#define CSR_MAX_RSC_LEN          16
#ifdef FEATURE_WLAN_WAPI
#define CSR_WAPI_BKID_SIZE          16
#define CSR_MAX_BKID_ALLOWED        16
#define CSR_WAPI_KEY_LEN        32
#define CSR_MAX_KEY_LEN         ( CSR_WAPI_KEY_LEN )  //longest one is for WAPI
#else
#define CSR_MAX_KEY_LEN         ( CSR_TKIP_KEY_LEN )  //longest one is for TKIP
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_ESE
#define CSR_KRK_KEY_LEN 16
#endif



typedef struct tagCsrChannelInfo
{
    tANI_U8 numOfChannels;
    tANI_U8 *ChannelList;   //it will be an array of channels
}tCsrChannelInfo, *tpCsrChannelInfo;

typedef struct tagCsrSSIDInfo
{
   tSirMacSSid     SSID;
   tANI_BOOLEAN    handoffPermitted;
   tANI_BOOLEAN    ssidHidden;
}tCsrSSIDInfo;

typedef struct tagCsrSSIDs
{
    tANI_U32 numOfSSIDs;
    tCsrSSIDInfo *SSIDList;   //To be allocated for array of SSIDs
}tCsrSSIDs;

typedef struct tagCsrBSSIDs
{
    tANI_U32 numOfBSSIDs;
    tCsrBssid *bssid;
}tCsrBSSIDs;

typedef struct tagCsrStaParams
{
    tANI_U16   capability;
    tANI_U8    extn_capability[SIR_MAC_MAX_EXTN_CAP];
    tANI_U8    supported_rates_len;
    tANI_U8    supported_rates[SIR_MAC_MAX_SUPP_RATES];
    tANI_U8    htcap_present;
    tSirHTCap  HTCap;
    tANI_U8    vhtcap_present;
    tSirVHTCap VHTCap;
    tANI_U8    uapsd_queues;
    tANI_U8    max_sp;
    tANI_U8    supported_channels_len;
    tANI_U8    supported_channels[SIR_MAC_MAX_SUPP_CHANNELS];
    tANI_U8    supported_oper_classes_len;
    tANI_U8    supported_oper_classes[SIR_MAC_MAX_SUPP_OPER_CLASSES];
}tCsrStaParams;

typedef struct tagCsrScanRequest
{
    tSirScanType scanType;
    tCsrBssid bssid;
    eCsrRoamBssType BSSType;
    tCsrSSIDs SSIDs;
    tCsrChannelInfo ChannelInfo;
    tANI_U32 minChnTime;    //in units of milliseconds
    tANI_U32 maxChnTime;    //in units of milliseconds
    tANI_U32 minChnTimeBtc;    //in units of milliseconds
    tANI_U32 maxChnTimeBtc;    //in units of milliseconds
    tANI_U32 restTime;      //in units of milliseconds  //ignored when not connected
    tANI_U32 uIEFieldLen;
    tANI_U8 *pIEField;
    eCsrRequestType requestType;    //11d scan or full scan
    tANI_BOOLEAN p2pSearch;
    tANI_BOOLEAN skipDfsChnlInP2pSearch;
}tCsrScanRequest;

typedef struct tagCsrBGScanRequest
{
    tSirScanType scanType;
    tSirMacSSid SSID;
    tCsrChannelInfo ChannelInfo;
    tANI_U32 scanInterval;  //in units of milliseconds
    tANI_U32 minChnTime;    //in units of milliseconds
    tANI_U32 maxChnTime;    //in units of milliseconds
    tANI_U32 minChnTimeBtc;    //in units of milliseconds
    tANI_U32 maxChnTimeBtc;    //in units of milliseconds
    tANI_U32 restTime;      //in units of milliseconds  //ignored when not connected
    tANI_U32 throughputImpact;      //specify whether BG scan cares about impacting throughput  //ignored when not connected
    tCsrBssid bssid;    //how to use it?? Apple
}tCsrBGScanRequest, *tpCsrBGScanRequest;


typedef struct tagCsrScanResultInfo
{
    //Carry the IEs for the current BSSDescription. A pointer to tDot11fBeaconIEs. Maybe NULL for start BSS.
    void *pvIes;
    tAniSSID ssId;
    v_TIME_t timer; // timer is variable which is used for hidden SSID's timer value
    //This member must be the last in the structure because the end of tSirBssDescription is an
    //    array with nonknown size at this time
    tSirBssDescription BssDescriptor;
}tCsrScanResultInfo;

typedef struct tagCsrEncryptionList
{

    tANI_U32 numEntries;
    eCsrEncryptionType encryptionType[eCSR_NUM_OF_ENCRYPT_TYPE];

}tCsrEncryptionList, *tpCsrEncryptionList;

typedef struct tagCsrAuthList
{
    tANI_U32 numEntries;
    eCsrAuthType authType[eCSR_NUM_OF_SUPPORT_AUTH_TYPE];
}tCsrAuthList, *tpCsrAuthList;

#ifdef WLAN_FEATURE_VOWIFI_11R
typedef struct tagCsrMobilityDomainInfo
{
    tANI_U8 mdiePresent;
    tANI_U16 mobilityDomain;
} tCsrMobilityDomainInfo;
#endif

#ifdef FEATURE_WLAN_ESE
typedef struct tagCsrEseCckmInfo
{
    tANI_U32       reassoc_req_num;
    tANI_BOOLEAN   krk_plumbed;
    tANI_U8        krk[CSR_KRK_KEY_LEN];
} tCsrEseCckmInfo;
#endif

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
#define CSR_DOT11F_IE_RSN_MAX_LEN   (114)  /*TODO: duplicate one in dot11f.h */

typedef struct tagCsrEseCckmIe
{
    tANI_U8 cckmIe[CSR_DOT11F_IE_RSN_MAX_LEN];
    tANI_U8 cckmIeLen;
} tCsrEseCckmIe;
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

typedef struct tagCsrScanResultFilter
{
    tCsrBSSIDs BSSIDs;    //each bssid has a length of WNI_CFG_BSSID_LEN (6)
    tCsrSSIDs SSIDs;
    tCsrChannelInfo ChannelInfo;
    tCsrAuthList authType;
    tCsrEncryptionList EncryptionType;
    //eCSR_ENCRYPT_TYPE_ANY cannot be set in multicast encryption type. If caller doesn't case,
    //put all supported encryption types in here
    tCsrEncryptionList mcEncryptionType;
    eCsrRoamBssType BSSType;
    //this is a bit mask of all the needed phy mode defined in eCsrPhyMode
    tANI_U32 phyMode;
    //If countryCode[0] is not 0, countryCode is checked independent of fCheckUnknownCountryCode
    tANI_U8 countryCode[WNI_CFG_COUNTRY_CODE_LEN];
    tANI_U8 uapsd_mask;
    /*For WPS filtering if true => auth and ecryption should be ignored*/
    tANI_BOOLEAN bWPSAssociation;
    tANI_BOOLEAN bOSENAssociation;
#if defined WLAN_FEATURE_VOWIFI
    /*For measurement reports --> if set, only SSID, BSSID and channel is considered for filtering.*/
    tANI_BOOLEAN fMeasurement;
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
    tCsrMobilityDomainInfo MDID;
#endif
    tANI_BOOLEAN p2pResult;
#ifdef WLAN_FEATURE_11W
    // Management Frame Protection
    tANI_BOOLEAN MFPEnabled;
    tANI_U8 MFPRequired;
    tANI_U8 MFPCapable;
#endif
}tCsrScanResultFilter;


typedef struct sCsrChnPower_
{
  tANI_U8 firstChannel;
  tANI_U8 numChannels;
  tANI_U8 maxtxPower;
}sCsrChnPower;


typedef struct sCsrChannel_
{
    tANI_U8 numChannels;
    tANI_U8 channelList[WNI_CFG_VALID_CHANNEL_LIST_LEN];
}sCsrChannel;


typedef struct tagCsr11dinfo
{
  sCsrChannel     Channels;
  tANI_U8         countryCode[WNI_CFG_COUNTRY_CODE_LEN+1];
  //max power channel list
  sCsrChnPower    ChnPower[WNI_CFG_VALID_CHANNEL_LIST_LEN];
}tCsr11dinfo;


typedef enum
{
    eCSR_ROAM_CANCELLED = 1,
    //this mean error happens before association_start or roaming_start is called.
    eCSR_ROAM_FAILED,
    //a CSR trigger roaming operation starts, callback may get a pointer to tCsrConnectedProfile
    eCSR_ROAM_ROAMING_START,
    //a CSR trigger roaming operation is completed
    eCSR_ROAM_ROAMING_COMPLETION,
    //Connection completed status.
    eCSR_ROAM_CONNECT_COMPLETION,
    //an association or start_IBSS operation starts,
    //callback may get a pointer to tCsrRoamProfile and a pointer to tSirBssDescription
    eCSR_ROAM_ASSOCIATION_START,
    //a roaming operation is finish, see eCsrRoamResult for
    //possible data passed back
    eCSR_ROAM_ASSOCIATION_COMPLETION,
    eCSR_ROAM_DISASSOCIATED,
    eCSR_ROAM_ASSOCIATION_FAILURE,
    //when callback with this flag. callback gets a pointer to the BSS desc.
    eCSR_ROAM_SHOULD_ROAM,
    //A new candidate for PMKID is found
    eCSR_ROAM_SCAN_FOUND_NEW_BSS,
    //CSR is done lostlink roaming and still cannot reconnect
    eCSR_ROAM_LOSTLINK,
    //a link lost is detected. CSR starts roaming.
    eCSR_ROAM_LOSTLINK_DETECTED,
    //TKIP MIC error detected, callback gets a pointer to tpSirSmeMicFailureInd
    eCSR_ROAM_MIC_ERROR_IND,
    eCSR_ROAM_IBSS_IND, //IBSS indications.
    //Update the connection status, useful for IBSS: new peer added, network is active etc.
    eCSR_ROAM_CONNECT_STATUS_UPDATE,
    eCSR_ROAM_GEN_INFO,
    eCSR_ROAM_SET_KEY_COMPLETE,
    eCSR_ROAM_REMOVE_KEY_COMPLETE,
    eCSR_ROAM_IBSS_LEAVE, //IBSS indications.
    //BSS in WDS mode status indication
    eCSR_ROAM_WDS_IND,
    //BSS in SoftAP mode status indication
    eCSR_ROAM_INFRA_IND,
    eCSR_ROAM_WPS_PBC_PROBE_REQ_IND,
#ifdef WLAN_FEATURE_VOWIFI_11R
    eCSR_ROAM_FT_RESPONSE,
#endif
    eCSR_ROAM_FT_START,
    eCSR_ROAM_REMAIN_CHAN_READY,
    eCSR_ROAM_SEND_ACTION_CNF,
    //this mean error happens before association_start or roaming_start is called.
    eCSR_ROAM_SESSION_OPENED,
    eCSR_ROAM_FT_REASSOC_FAILED,
#ifdef FEATURE_WLAN_LFR
    eCSR_ROAM_PMK_NOTIFY,
#endif
#ifdef FEATURE_WLAN_LFR_METRICS
    eCSR_ROAM_PREAUTH_INIT_NOTIFY,
    eCSR_ROAM_PREAUTH_STATUS_SUCCESS,
    eCSR_ROAM_PREAUTH_STATUS_FAILURE,
    eCSR_ROAM_HANDOVER_SUCCESS,
#endif
#ifdef FEATURE_WLAN_TDLS
    eCSR_ROAM_TDLS_STATUS_UPDATE,
    eCSR_ROAM_RESULT_MGMT_TX_COMPLETE_IND,
#endif
    eCSR_ROAM_DISCONNECT_ALL_P2P_CLIENTS, //Disaconnect all the clients
    eCSR_ROAM_SEND_P2P_STOP_BSS, //Stopbss triggered from SME due to different
                                 // beacon interval
#ifdef WLAN_FEATURE_11W
    eCSR_ROAM_UNPROT_MGMT_FRAME_IND,
#endif

#ifdef WLAN_FEATURE_AP_HT40_24G
    eCSR_ROAM_2040_COEX_INFO_IND,
#endif

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
    eCSR_ROAM_TSM_IE_IND,
    eCSR_ROAM_CCKM_PREAUTH_NOTIFY,
    eCSR_ROAM_ESE_ADJ_AP_REPORT_IND,
    eCSR_ROAM_ESE_BCN_REPORT_IND,
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
    eCSR_ROAM_UPDATE_MAX_RATE_IND,
    eCSR_ROAM_LOST_LINK_PARAMS_IND,
}eRoamCmdStatus;


//comment inside indicates what roaming callback gets
typedef enum
{
    eCSR_ROAM_RESULT_NONE,
    //this means no more action in CSR
    //If roamStatus is eCSR_ROAM_ASSOCIATION_COMPLETION, tCsrRoamInfo's pBssDesc may pass back
    eCSR_ROAM_RESULT_FAILURE,
    //Pass back pointer to tCsrRoamInfo
    eCSR_ROAM_RESULT_ASSOCIATED,
    eCSR_ROAM_RESULT_NOT_ASSOCIATED,
    eCSR_ROAM_RESULT_MIC_FAILURE,
    eCSR_ROAM_RESULT_FORCED,
    eCSR_ROAM_RESULT_DISASSOC_IND,
    eCSR_ROAM_RESULT_DEAUTH_IND,
    eCSR_ROAM_RESULT_CAP_CHANGED,
    //This means we starts an IBSS
    //tCsrRoamInfo's pBssDesc may pass back
    eCSR_ROAM_RESULT_IBSS_STARTED,
    //START_BSS failed
    //tCsrRoamInfo's pBssDesc may pass back
    eCSR_ROAM_RESULT_IBSS_START_FAILED,
    eCSR_ROAM_RESULT_IBSS_JOIN_SUCCESS,
    eCSR_ROAM_RESULT_IBSS_JOIN_FAILED,
    eCSR_ROAM_RESULT_IBSS_CONNECT,
    eCSR_ROAM_RESULT_IBSS_INACTIVE,
    //If roamStatus is eCSR_ROAM_ASSOCIATION_COMPLETION
    //tCsrRoamInfo's pBssDesc may pass back. and the peer's MAC address in peerMacOrBssid
    //If roamStatus is eCSR_ROAM_IBSS_IND,
    //the peer's MAC address in peerMacOrBssid and a beacon frame of the IBSS in pbFrames
    eCSR_ROAM_RESULT_IBSS_NEW_PEER,
    //Peer departed from IBSS, Callback may get a pointer tSmeIbssPeerInd in pIbssPeerInd
    eCSR_ROAM_RESULT_IBSS_PEER_DEPARTED,
    //Coalescing in the IBSS network (joined an IBSS network)
    //Callback pass a BSSID in peerMacOrBssid
    eCSR_ROAM_RESULT_IBSS_COALESCED,
    //If roamStatus is eCSR_ROAM_ROAMING_START, callback may get a pointer to tCsrConnectedProfile used to connect.
    eCSR_ROAM_RESULT_IBSS_STOP,
    eCSR_ROAM_RESULT_LOSTLINK,
    eCSR_ROAM_RESULT_MIC_ERROR_UNICAST,
    eCSR_ROAM_RESULT_MIC_ERROR_GROUP,
    eCSR_ROAM_RESULT_AUTHENTICATED,
    eCSR_ROAM_RESULT_NEW_RSN_BSS,
#ifdef FEATURE_WLAN_WAPI
    eCSR_ROAM_RESULT_NEW_WAPI_BSS,
#endif /* FEATURE_WLAN_WAPI */
    // WDS started successfully
    eCSR_ROAM_RESULT_WDS_STARTED,
    // WDS start failed
    eCSR_ROAM_RESULT_WDS_START_FAILED,
    // WDS stopped
    eCSR_ROAM_RESULT_WDS_STOPPED,
    // WDS joined successfully in STA mode
    eCSR_ROAM_RESULT_WDS_ASSOCIATED,
    // A station joined WDS AP
    eCSR_ROAM_RESULT_WDS_ASSOCIATION_IND,
    // WDS join failed in STA mode
    eCSR_ROAM_RESULT_WDS_NOT_ASSOCIATED,
    // WDS disassociated
    eCSR_ROAM_RESULT_WDS_DISASSOCIATED,
    // INFRA started successfully
    eCSR_ROAM_RESULT_INFRA_STARTED,
    // INFRA start failed
    eCSR_ROAM_RESULT_INFRA_START_FAILED,
    // INFRA stopped
    eCSR_ROAM_RESULT_INFRA_STOPPED,
    // A station joining INFRA AP
    eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND,
    // A station joined INFRA AP
    eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF,
    // INFRA disassociated
    eCSR_ROAM_RESULT_INFRA_DISASSOCIATED,
    eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND,
    eCSR_ROAM_RESULT_SEND_ACTION_FAIL,
    // peer rejected assoc because max assoc limit reached. callback gets pointer to peer
    eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED,
    //Assoc rejected due to concurrent session running on a different channel
    eCSR_ROAM_RESULT_ASSOC_FAIL_CON_CHANNEL,
#ifdef FEATURE_WLAN_TDLS
    eCSR_ROAM_RESULT_ADD_TDLS_PEER,
    eCSR_ROAM_RESULT_UPDATE_TDLS_PEER,
    eCSR_ROAM_RESULT_DELETE_TDLS_PEER,
    eCSR_ROAM_RESULT_TEARDOWN_TDLS_PEER_IND,
    eCSR_ROAM_RESULT_DELETE_ALL_TDLS_PEER_IND,
    eCSR_ROAM_RESULT_LINK_ESTABLISH_REQ_RSP,
#endif

}eCsrRoamResult;



/*----------------------------------------------------------------------------
  List of link quality indications HDD can receive from SME
-----------------------------------------------------------------------------*/
typedef enum
{
 eCSR_ROAM_LINK_QUAL_MIN_IND     = -1,

 eCSR_ROAM_LINK_QUAL_POOR_IND            =  0,   /* bad link                */
 eCSR_ROAM_LINK_QUAL_GOOD_IND            =  1,   /* acceptable for voice    */
 eCSR_ROAM_LINK_QUAL_VERY_GOOD_IND       =  2,   /* suitable for voice      */
 eCSR_ROAM_LINK_QUAL_EXCELLENT_IND       =  3,   /* suitable for voice      */

 eCSR_ROAM_LINK_QUAL_MAX_IND  /* invalid value */

} eCsrRoamLinkQualityInd;

typedef enum
{
    eCSR_DISCONNECT_REASON_UNSPECIFIED = 0,
    eCSR_DISCONNECT_REASON_MIC_ERROR,
    eCSR_DISCONNECT_REASON_DISASSOC,
    eCSR_DISCONNECT_REASON_DEAUTH,
    eCSR_DISCONNECT_REASON_HANDOFF,
    eCSR_DISCONNECT_REASON_IBSS_JOIN_FAILURE,
    eCSR_DISCONNECT_REASON_IBSS_LEAVE,
}eCsrRoamDisconnectReason;

typedef enum
{
    // Not associated in Infra or participating in an IBSS / Ad-hoc network.
    eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED,
    // Associated in an Infrastructure network.
    eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED,
    // Participating in an IBSS network though disconnected (no partner stations
    // in the IBSS).
    eCSR_ASSOC_STATE_TYPE_IBSS_DISCONNECTED,
    // Participating in an IBSS network with partner stations also present
    eCSR_ASSOC_STATE_TYPE_IBSS_CONNECTED,
    // Participating in a WDS network in AP or STA mode but not connected yet
    eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED,
    // Participating in a WDS network and connected peer to peer
    eCSR_ASSOC_STATE_TYPE_WDS_CONNECTED,
    // Participating in a Infra network in AP not yet in connected state
    eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED,
    // Participating in a Infra network and connected to a peer
    eCSR_ASSOC_STATE_TYPE_INFRA_CONNECTED,

}eCsrConnectState;


// This parameter is no longer supported in the Profile.  Need to set this in the global properties
// for the adapter.
typedef enum eCSR_MEDIUM_ACCESS
{
    eCSR_MEDIUM_ACCESS_AUTO = 0,
    eCSR_MEDIUM_ACCESS_DCF,
    eCSR_MEDIUM_ACCESS_eDCF,
    eCSR_MEDIUM_ACCESS_HCF,

    eCSR_MEDIUM_ACCESS_WMM_eDCF_802dot1p,
    eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP,
    eCSR_MEDIUM_ACCESS_WMM_eDCF_NoClassify,
    eCSR_MEDIUM_ACCESS_11e_eDCF = eCSR_MEDIUM_ACCESS_eDCF,
    eCSR_MEDIUM_ACCESS_11e_HCF  = eCSR_MEDIUM_ACCESS_HCF,
}eCsrMediaAccessType;

typedef enum
{
    eCSR_TX_RATE_AUTO = 0,   // use rate adaption to determine Tx rate.

    eCSR_TX_RATE_1Mbps   = 0x00000001,
    eCSR_TX_RATE_2Mbps   = 0x00000002,
    eCSR_TX_RATE_5_5Mbps = 0x00000004,
    eCSR_TX_RATE_6Mbps   = 0x00000008,
    eCSR_TX_RATE_9Mbps   = 0x00000010,
    eCSR_TX_RATE_11Mbps  = 0x00000020,
    eCSR_TX_RATE_12Mbps  = 0x00000040,
    eCSR_TX_RATE_18Mbps  = 0x00000080,
    eCSR_TX_RATE_24Mbps  = 0x00000100,
    eCSR_TX_RATE_36Mbps  = 0x00000200,
    eCSR_TX_RATE_42Mbps  = 0x00000400,
    eCSR_TX_RATE_48Mbps  = 0x00000800,
    eCSR_TX_RATE_54Mbps  = 0x00001000,
    eCSR_TX_RATE_72Mbps  = 0x00002000,
    eCSR_TX_RATE_84Mbps  = 0x00004000,
    eCSR_TX_RATE_96Mbps  = 0x00008000,
    eCSR_TX_RATE_108Mbps = 0x00010000,
    eCSR_TX_RATE_126Mbps = 0x00020000,
    eCSR_TX_RATE_144Mbps = 0x00040000,
    eCSR_TX_RATE_168Mbps = 0x00080000,
    eCSR_TX_RATE_192Mbps = 0x00100000,
    eCSR_TX_RATE_216Mbps = 0x00200000,
    eCSR_TX_RATE_240Mbps = 0x00400000,

}eCsrExposedTxRate;

typedef enum
{
    eCSR_OPERATING_CHANNEL_ALL  = 0,
    eCSR_OPERATING_CHANNEL_AUTO = eCSR_OPERATING_CHANNEL_ALL,
    eCSR_OPERATING_CHANNEL_ANY  = eCSR_OPERATING_CHANNEL_ALL,
}eOperationChannel;

typedef enum
{
    eCSR_DOT11_FRAG_THRESH_AUTO            = -1,
    eCSR_DOT11_FRAG_THRESH_MIN             = 256,
    eCSR_DOT11_FRAG_THRESH_MAX             = 2346,
    eCSR_DOT11_FRAG_THRESH_DEFAULT         = 2000
}eCsrDot11FragThresh;


//for channel bonding for ibss
typedef enum
{
    eCSR_CB_OFF = 0,
    eCSR_CB_AUTO = 1,
    eCSR_CB_DOWN = 2,
    eCSR_CB_UP = 3,
}eCsrCBChoice;

//For channel bonding, the channel number gap is 4, either up or down. For both 11a and 11g mode.
#define CSR_CB_CHANNEL_GAP 4
#define CSR_CB_CENTER_CHANNEL_OFFSET    2
#define CSR_MAX_24GHz_CHANNEL_NUMBER ( SIR_11B_CHANNEL_END )
#define CSR_MIN_5GHz_CHANNEL_NUMBER  ( SIR_11A_CHANNEL_BEGIN )
#define CSR_MAX_5GHz_CHANNEL_NUMBER  ( SIR_11A_CHANNEL_END )

// WEP keysize (in bits)...
typedef enum
{
    eCSR_SECURITY_WEP_KEYSIZE_40  =  40,   // 40 bit key + 24bit IV = 64bit WEP
    eCSR_SECURITY_WEP_KEYSIZE_104 = 104,   // 104bit key + 24bit IV = 128bit WEP

    eCSR_SECURITY_WEP_KEYSIZE_MIN = eCSR_SECURITY_WEP_KEYSIZE_40,
    eCSR_SECURITY_WEP_KEYSIZE_MAX = eCSR_SECURITY_WEP_KEYSIZE_104,
    eCSR_SECURITY_WEP_KEYSIZE_MAX_BYTES = ( eCSR_SECURITY_WEP_KEYSIZE_MAX / 8 ),
}eCsrWEPKeySize;


// Possible values for the WEP static key ID...
typedef enum
{

    eCSR_SECURITY_WEP_STATIC_KEY_ID_MIN       =  0,
    eCSR_SECURITY_WEP_STATIC_KEY_ID_MAX       =  3,
    eCSR_SECURITY_WEP_STATIC_KEY_ID_DEFAULT   =  0,

    eCSR_SECURITY_WEP_STATIC_KEY_ID_INVALID   = -1,

}eCsrWEPStaticKeyID;

// Two extra key indicies are used for the IGTK (which is used by BIP)
#define CSR_MAX_NUM_KEY     (eCSR_SECURITY_WEP_STATIC_KEY_ID_MAX + 2 + 1)

typedef enum
{
    eCSR_SECURITY_SET_KEY_ACTION_NO_CHANGE,
    eCSR_SECURITY_SET_KEY_ACTION_SET_KEY,
    eCSR_SECURITY_SET_KEY_ACTION_DELETE_KEY,
}eCsrSetKeyAction;

typedef enum
{
    eCSR_BAND_ALL,
    eCSR_BAND_24,
    eCSR_BAND_5G,
    eCSR_BAND_MAX,
}eCsrBand;


typedef enum
{
   // Roaming because HDD requested for reassoc by changing one of the fields in
   // tCsrRoamModifyProfileFields. OR
   // Roaming because SME requested for reassoc by changing one of the fields in
   // tCsrRoamModifyProfileFields.
   eCsrRoamReasonStaCapabilityChanged,
   // Roaming because SME requested for reassoc to a different AP, as part of
   // inter AP handoff.
   eCsrRoamReasonBetterAP,
   // Roaming because SME requested it as the link is lost - placeholder, will
   // clean it up once handoff code gets in
   eCsrRoamReasonSmeIssuedForLostLink,

}eCsrRoamReasonCodes;

typedef enum
{
   eCsrRoamWmmAuto = 0,
   eCsrRoamWmmQbssOnly = 1,
   eCsrRoamWmmNoQos = 2,

} eCsrRoamWmmUserModeType;

typedef enum
{
   eCSR_REQUESTER_MIN = 0,
   eCSR_DIAG,
   eCSR_UMA_GAN,
   eCSR_HDD
} eCsrStatsRequesterType;

typedef enum
{
    INIT = 0,
    REINIT,
} driver_load_type;

typedef struct tagPmkidCandidateInfo
{
    tCsrBssid BSSID;
    tANI_BOOLEAN preAuthSupported;
}tPmkidCandidateInfo;

typedef struct tagPmkidCacheInfo
{
    tCsrBssid BSSID;
    tANI_U8 PMKID[CSR_RSN_PMKID_SIZE];
}tPmkidCacheInfo;

#ifdef FEATURE_WLAN_WAPI
typedef struct tagBkidCandidateInfo
{
    tCsrBssid BSSID;
    tANI_BOOLEAN preAuthSupported;
}tBkidCandidateInfo;

typedef struct tagBkidCacheInfo
{
    tCsrBssid BSSID;
    tANI_U8 BKID[CSR_WAPI_BKID_SIZE];
}tBkidCacheInfo;
#endif /* FEATURE_WLAN_WAPI */

typedef struct tagCsrKeys
{
    tANI_U8 KeyLength[ CSR_MAX_NUM_KEY ];   //Also use to indicate whether the key index is set
    tANI_U8 KeyMaterial[ CSR_MAX_NUM_KEY ][ CSR_MAX_KEY_LEN ];
    tANI_U8 defaultIndex;
}tCsrKeys;

/* Following are fields which are part of tCsrRoamConnectedProfile might need
   modification dynamically once STA is up & running and this could trigger
   reassoc */
typedef struct tagCsrRoamModifyProfileFields
{
   // during connect this specifies ACs U-APSD is to be setup
   //   for (Bit0:VO; Bit1:VI; Bit2:BK; Bit3:BE all other bits are ignored).
   //  During assoc response this COULD carry confirmation of what ACs U-APSD
   // got setup for. Later if an APP looking for APSD, SME-QoS might need to
   // modify this field
   tANI_U8     uapsd_mask;
   // HDD might ask to modify this field
   tANI_U16    listen_interval;
}tCsrRoamModifyProfileFields;

typedef struct tagCsrRoamProfile
{
    //For eCSR_BSS_TYPE_WDS_AP. There must be one SSID in SSIDs.
    //For eCSR_BSS_TYPE_WDS_STA. There must be two SSIDs. Index 0 is the SSID of the WDS-AP
    //that we need to join. Index 1 is the SSID for self BSS.
    tCsrSSIDs SSIDs;
    tCsrBSSIDs BSSIDs;
    tANI_U32 phyMode;   //this is a bit mask of all the needed phy mode defined in eCsrPhyMode
    eCsrRoamBssType BSSType;

    tCsrAuthList AuthType;
    eCsrAuthType negotiatedAuthType;

    tCsrEncryptionList EncryptionType;
    //This field is for output only, not for input
    eCsrEncryptionType negotiatedUCEncryptionType;

    //eCSR_ENCRYPT_TYPE_ANY cannot be set in multicast encryption type. If caller doesn't case,
    //put all supported encryption types in here
    tCsrEncryptionList mcEncryptionType;
    //This field is for output only, not for input
    eCsrEncryptionType negotiatedMCEncryptionType;

#ifdef WLAN_FEATURE_11W
    // Management Frame Protection
    tANI_BOOLEAN MFPEnabled;
    tANI_U8 MFPRequired;
    tANI_U8 MFPCapable;
#endif

    tCsrKeys Keys;
    eCsrCBChoice CBMode; //up, down or auto
    tCsrChannelInfo ChannelInfo;
    tANI_U8 operationChannel;
    tANI_U16 beaconInterval;    //If this is 0, SME will fill in for caller.
    // during connect this specifies ACs U-APSD is to be setup
    //   for (Bit0:VO; Bit1:VI; Bit2:BK; Bit3:BE all other bits are ignored).
    //  During assoc response this COULD carry confirmation of what ACs U-APSD got setup for
    tANI_U8 uapsd_mask;
    tANI_U32 nWPAReqIELength;   //The byte count in the pWPAReqIE
    tANI_U8 *pWPAReqIE;   //If not null, it has the IE byte stream for WPA
    tANI_U32 nRSNReqIELength;  //The byte count in the pRSNReqIE
    tANI_U8 *pRSNReqIE;     //If not null, it has the IE byte stream for RSN
#ifdef FEATURE_WLAN_WAPI
    tANI_U32 nWAPIReqIELength;   //The byte count in the pWAPIReqIE
    tANI_U8 *pWAPIReqIE;   //If not null, it has the IE byte stream for WAPI
#endif /* FEATURE_WLAN_WAPI */

    //The byte count in the pAddIE for scan (at the time of join)
    tANI_U32 nAddIEScanLength;
    /* Additional IE information.
     * It has the IE byte stream for additional IE,
     * which can be WSC IE and/or P2P IE
     */
    tANI_U8  addIEScan[SIR_MAC_MAX_ADD_IE_LENGTH+2];       //Additional IE information.
    tANI_U32 nAddIEAssocLength;   //The byte count in the pAddIE for assoc
    tANI_U8 *pAddIEAssoc;       //If not null, it has the IE byte stream for additional IE, which can be WSC IE and/or P2P IE

    tANI_U8 countryCode[WNI_CFG_COUNTRY_CODE_LEN];  //it is ignored if [0] is 0.
    /*WPS Association if true => auth and ecryption should be ignored*/
    tANI_BOOLEAN bWPSAssociation;
    tANI_BOOLEAN bOSENAssociation;
    tANI_U32 nWSCReqIELength;   //The byte count in the pWSCReqIE
    tANI_U8 *pWSCReqIE;   //If not null, it has the IE byte stream for WSC

    tANI_U8 ieee80211d;
    tANI_U8 privacy;
    tANI_BOOLEAN fwdWPSPBCProbeReq;
    tAniAuthType csr80211AuthType;
    tANI_U32 dtimPeriod;
    tANI_BOOLEAN ApUapsdEnable;
    tANI_BOOLEAN protEnabled;
    tANI_BOOLEAN obssProtEnabled;
    tANI_U16 cfg_protection;
    tANI_U8 wps_state;

#ifdef WLAN_FEATURE_VOWIFI_11R
    tCsrMobilityDomainInfo MDID;
#endif
    tVOS_CON_MODE csrPersona;

}tCsrRoamProfile;


typedef struct tagCsrRoamConnectedProfile
{
    tSirMacSSid SSID;
    tANI_BOOLEAN    handoffPermitted;
    tANI_BOOLEAN    ssidHidden;
    tCsrBssid bssid;
    eCsrRoamBssType BSSType;
    eCsrAuthType AuthType;
    tCsrAuthList AuthInfo;
    eCsrEncryptionType EncryptionType;
    tCsrEncryptionList EncryptionInfo;
    eCsrEncryptionType mcEncryptionType;
    tCsrEncryptionList mcEncryptionInfo;
    eCsrCBChoice CBMode; //up, down or auto
    tANI_U8 operationChannel;
    tANI_U16   beaconInterval;
    tCsrKeys Keys;
    // meaningless on connect. It's an OUT param from CSR's point of view
    // During assoc response carries the ACM bit-mask i.e. what
    // ACs have ACM=1 (if any),
    // (Bit0:VO; Bit1:VI; Bit2:BK; Bit3:BE all other bits are ignored)
    tANI_U8  acm_mask;
    tCsrRoamModifyProfileFields modifyProfileFields;
    tANI_U32 nAddIEAssocLength;   //The byte count in the pAddIE for assoc
    tANI_U8 *pAddIEAssoc;       //If not null, it has the IE byte stream for additional IE, which can be WSC IE and/or P2P IE

    tSirBssDescription *pBssDesc;
    tANI_BOOLEAN   qap; //AP supports QoS
    tANI_BOOLEAN   qosConnection; //A connection is QoS enabled
#ifdef WLAN_FEATURE_VOWIFI_11R
    tCsrMobilityDomainInfo MDID;
#endif

#ifdef FEATURE_WLAN_ESE
    tCsrEseCckmInfo eseCckmInfo;
    tANI_BOOLEAN    isESEAssoc;
#endif
    tANI_U32 dot11Mode;

#ifdef WLAN_FEATURE_11W
    /* Management Frame Protection */
    tANI_BOOLEAN MFPEnabled;
    tANI_U8 MFPRequired;
    tANI_U8 MFPCapable;
#endif

}tCsrRoamConnectedProfile;


#ifdef WLAN_FEATURE_VOWIFI_11R
typedef struct tagCsr11rConfigParams
{
    tANI_BOOLEAN   IsFTResourceReqSupported;
} tCsr11rConfigParams;
#endif

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
typedef struct tagCsrNeighborRoamConfigParams
{

    tANI_U32       nNeighborScanTimerPeriod;
    tANI_U8        nNeighborLookupRssiThreshold;
    tANI_U8        nNeighborReassocRssiThreshold;
    tANI_U16       nNeighborScanMinChanTime;
    tANI_U16       nNeighborScanMaxChanTime;
    sCsrChannel    neighborScanChanList;
    tANI_U8        nMaxNeighborRetries;
    tANI_U16       nNeighborResultsRefreshPeriod;
    tANI_U16       nEmptyScanRefreshPeriod;
    tANI_U8        nNeighborInitialForcedRoamTo5GhEnable;
}tCsrNeighborRoamConfigParams;
#endif

typedef struct tagCsrConfigParam
{
    tANI_U32 FragmentationThreshold;
    tANI_U32 channelBondingMode24GHz;   // keep this tANI_U32. This gets converted to ePhyChannelBondState
    tANI_U32 channelBondingMode5GHz;    // in csrChangeDefaultConfigParam using convertCBIniValueToPhyCBState
    eCsrPhyMode phyMode;
    eCsrBand eBand;
    tANI_U32 RTSThreshold;
    tANI_U32 HeartbeatThresh50;
    tANI_U32 HeartbeatThresh24;
    eCsrCBChoice cbChoice;
    eCsrBand bandCapability;     //indicate hw capability
    tANI_U32 bgScanInterval;
    tANI_U16 TxRate;
    eCsrRoamWmmUserModeType WMMSupportMode;
    tANI_BOOLEAN Is11eSupportEnabled;
    tANI_BOOLEAN Is11dSupportEnabled;
    tANI_BOOLEAN Is11dSupportEnabledOriginal;
    tANI_BOOLEAN Is11hSupportEnabled;
    tANI_BOOLEAN shortSlotTime;
    tANI_BOOLEAN ProprietaryRatesEnabled;
    tANI_U8 AdHocChannel24;
    tANI_U8 AdHocChannel5G;
    tANI_U32 impsSleepTime;     //in units of seconds
    tANI_U32 nScanResultAgeCount;   //this number minus one is the number of times a scan doesn't find it before it is removed
    tANI_U32 scanAgeTimeNCNPS;  //scan result aging time threshold when Not-Connect-No-Power-Save, in seconds
    tANI_U32 scanAgeTimeNCPS;   //scan result aging time threshold when Not-Connect-Power-Save, in seconds
    tANI_U32 scanAgeTimeCNPS;   //scan result aging time threshold when Connect-No-Power-Save, in seconds,
    tANI_U32 scanAgeTimeCPS;   //scan result aging time threshold when Connect-Power-Savein seconds
    tANI_U32 nRoamingTime;  //In seconds, CSR will try this long before gives up. 0 means no roaming
    tANI_U8 bCatRssiOffset;     //to set the RSSI difference for each category
    tANI_U8 fEnableMCCMode; //to set MCC Enable/Disable mode
    tANI_U8 fAllowMCCGODiffBI; //to allow MCC GO different B.I than STA's. NOTE: make sure if RIVA firmware can handle this combination before enabling this
                               //at the moment, this flag is provided only to pass Wi-Fi Cert. 5.1.12
    tCsr11dinfo  Csr11dinfo;
    //Whether to limit the channels to the ones set in Csr11dInfo. If true, the opertaional
    //channels are limited to the default channel list. It is an "AND" operation between the
    //default channels and the channels in the 802.11d IE.
    tANI_BOOLEAN fEnforce11dChannels;
    //Country Code Priority
    //0 = 802.11D > Country IOCTL > NV
    //1 = Country IOCTL > 802.11D > NV
    tANI_BOOLEAN fSupplicantCountryCodeHasPriority;
    //When true, AP with unknown country code won't be see.
    //"Unknown country code" means either Ap doesn't have 11d IE or we cannot
    //find a domain for the country code in its 11d IE.
    tANI_BOOLEAN fEnforceCountryCodeMatch;
    //When true, only APs in the default domain can be seen. If the Ap has "unknown country
    //code", or the domain of the country code doesn't match the default domain, the Ap is
    //not acceptable.
    tANI_BOOLEAN fEnforceDefaultDomain;

    tANI_U16 vccRssiThreshold;
    tANI_U32 vccUlMacLossThreshold;

    tANI_U32  nPassiveMinChnTime;    //in units of milliseconds
    tANI_U32  nPassiveMaxChnTime;    //in units of milliseconds
    tANI_U32  nActiveMinChnTime;     //in units of milliseconds
    tANI_U32  nActiveMaxChnTime;     //in units of milliseconds

    tANI_U32  nInitialDwellTime;      //in units of milliseconds

    tANI_U32  nActiveMinChnTimeBtc;     //in units of milliseconds
    tANI_U32  nActiveMaxChnTimeBtc;     //in units of milliseconds
    tANI_U32  disableAggWithBtc;
#ifdef WLAN_AP_STA_CONCURRENCY
    tANI_U32  nPassiveMinChnTimeConc;    //in units of milliseconds
    tANI_U32  nPassiveMaxChnTimeConc;    //in units of milliseconds
    tANI_U32  nActiveMinChnTimeConc;     //in units of milliseconds
    tANI_U32  nActiveMaxChnTimeConc;     //in units of milliseconds
    tANI_U32  nRestTimeConc;             //in units of milliseconds
    tANI_U8   nNumStaChanCombinedConc;   //number of channels combined for
                                         //STA in each split scan operation
    tANI_U8   nNumP2PChanCombinedConc;   //number of channels combined for
                                         //P2P in each split scan operation
#endif

    tANI_BOOLEAN IsIdleScanEnabled;
    //in dBm, the maximum TX power
    //The actual TX power is the lesser of this value and 11d.
    //If 11d is disable, the lesser of this and default setting.
    tANI_U8 nTxPowerCap;
    tANI_U32  statsReqPeriodicity;  //stats request frequency from PE while in full power
    tANI_U32  statsReqPeriodicityInPS;//stats request frequency from PE while in power save
#ifdef WLAN_FEATURE_VOWIFI_11R
    tCsr11rConfigParams  csr11rConfig;
#endif
#ifdef FEATURE_WLAN_ESE
    tANI_U8   isEseIniFeatureEnabled;
#endif
#ifdef FEATURE_WLAN_LFR
    tANI_U8   isFastRoamIniFeatureEnabled;
    tANI_U8   MAWCEnabled;
#endif

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
    tANI_U8        isFastTransitionEnabled;
    tANI_U8        RoamRssiDiff;
    tANI_U8        nImmediateRoamRssiDiff;
    tANI_BOOLEAN   isWESModeEnabled;
#endif

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
    tCsrNeighborRoamConfigParams    neighborRoamConfig;
#endif

    /* Instead of Reassoc, send ADDTS/DELTS even when ACM is off for that AC
     * This is mandated by WMM-AC certification */
    tANI_BOOLEAN addTSWhenACMIsOff;


    /*channelPowerInfoList24 has been seen corrupted. Set this flag to true trying to
    * detect when it happens. Adding this into code because we can't reproduce it easily.
    * We don't know when it happens. */
    tANI_BOOLEAN fValidateList;

    /*Customer wants to start with an active scan based on the default country code.
    * This optimization will minimize the driver load to association time.
    * Based on this flag we will bypass the initial passive scan needed for 11d
    * to determine the country code & domain */
    tANI_BOOLEAN fEnableBypass11d;

    /*Customer wants to optimize the scan time. Avoiding scans(passive) on DFS
    * channels while swipping through both bands can save some time
    * (apprx 1.3 sec) */
    tANI_U8 fEnableDFSChnlScan;

    //To enable/disable scanning 2.4Ghz channels twice on a single scan request from HDD
    tANI_BOOLEAN fScanTwice;
#ifdef WLAN_FEATURE_11AC
    tANI_U32        nVhtChannelWidth;
    tANI_U8         enableTxBF;
    tANI_U8         txBFCsnValue;
    tANI_BOOLEAN    enableVhtFor24GHz;
    tANI_U8         enableMuBformee;
#endif

    /*
    * To enable/disable scanning only 2.4Ghz channels on first scan
    */
    tANI_BOOLEAN fFirstScanOnly2GChnl;

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
    tANI_BOOLEAN nRoamPrefer5GHz;
    tANI_BOOLEAN nRoamIntraBand;
    tANI_U8      nProbes;
    tANI_U16     nRoamScanHomeAwayTime;

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    tANI_BOOLEAN isRoamOffloadScanEnabled;
    tANI_BOOLEAN bFastRoamInConIniFeatureEnabled;
#endif
#endif

    tANI_BOOLEAN ignorePeerErpInfo;
    tANI_U8 scanCfgAgingTime;

    tANI_U8   enableTxLdpc;

    tANI_U8 isAmsduSupportInAMPDU;
    tANI_U8 nSelect5GHzMargin;

    tANI_U8 isCoalesingInIBSSAllowed;
    tANI_U8 allowDFSChannelRoam;
    tANI_BOOLEAN initialScanSkipDFSCh;
    tANI_BOOLEAN sendDeauthBeforeCon;

    eCsrBand  scanBandPreference;
#ifdef WLAN_FEATURE_AP_HT40_24G
    tANI_BOOLEAN apHT40_24GEnabled;
    tANI_U32 channelBondingAPMode24GHz; // Use for SAP/P2P GO 2.4GHz channel Bonding
#endif
    tANI_U32 nOBSSScanWidthTriggerInterval;
    tANI_U8 roamDelayStatsEnabled;
    tANI_BOOLEAN ignorePeerHTopMode;
    tANI_BOOLEAN disableP2PMacSpoofing;
}tCsrConfigParam;

//Tush
typedef struct tagCsrUpdateConfigParam
{
   tCsr11dinfo  Csr11dinfo;
}tCsrUpdateConfigParam;

typedef struct tagCsrRoamInfo
{
    tCsrRoamProfile *pProfile;  //may be NULL
    tSirBssDescription *pBssDesc;  //May be NULL
    tANI_U32 nBeaconLength; //the length, in bytes, of the beacon frame, can be 0
    tANI_U32 nAssocReqLength;   //the length, in bytes, of the assoc req frame, can be 0
    tANI_U32 nAssocRspLength;   //The length, in bytes, of the assoc rsp frame, can be 0
    tANI_U32 nFrameLength;
    tANI_U8  frameType;
    tANI_U8 *pbFrames;  //Point to a buffer contain the beacon, assoc req, assoc rsp frame, in that order
                        //user needs to use nBeaconLength, nAssocReqLength, nAssocRspLength to desice where
                        //each frame starts and ends.
    tANI_BOOLEAN fReassocReq;   //set to true if for re-association
    tANI_BOOLEAN fReassocRsp;   //set to true if for re-association
    tCsrBssid bssid;
    //Only valid in IBSS
    //this is the peers MAC address for eCSR_ROAM_RESULT_IBSS_NEW_PEER or PEER_DEPARTED
    tCsrBssid peerMac;
    tSirResultCodes statusCode;
    tANI_U32 reasonCode;    //this could be our own defined or sent from the other BSS(per 802.11 spec)
    tANI_U8  staId;         // Peer stationId when connected
    /*The DPU signatures will be sent eventually to TL to help it determine the
      association to which a packet belongs to*/
    /*Unicast DPU signature*/
    tANI_U8            ucastSig;

    /*Broadcast DPU signature*/
    tANI_U8            bcastSig;

    tANI_BOOLEAN fAuthRequired;   //FALSE means auth needed from supplicant. TRUE means authenticated(static WEP, open)
    tANI_U8 sessionId;
    tANI_U8 rsnIELen;
    tANI_U8 *prsnIE;

    tANI_U8 addIELen;
    tANI_U8 *paddIE;

    union
    {
        tSirMicFailureInfo *pMICFailureInfo;
        tCsrRoamConnectedProfile *pConnectedProfile;
        tSirWPSPBCProbeReq *pWPSPBCProbeReq;
        tSirLostLinkParamsInfo *pLostLinkParams;
    } u;

    tANI_BOOLEAN wmmEnabledSta;   //set to true if WMM enabled STA
#ifdef WLAN_FEATURE_AP_HT40_24G
    tANI_BOOLEAN HT40MHzIntoEnabledSta; //set to true if 40 MHz Intolerant enabled STA
#endif
    tANI_U32 dtimPeriod;

#ifdef FEATURE_WLAN_ESE
    tANI_BOOLEAN isESEAssoc;
#ifdef FEATURE_WLAN_ESE_UPLOAD
    tSirTsmIE tsmIe;
    tANI_U32 timestamp[2];
    tANI_U16 tsmRoamDelay;
    tSirEseBcnReportRsp *pEseBcnReportRsp;
#endif /* FEATURE_WLAN_ESE_UPLOAD */
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
    tANI_BOOLEAN is11rAssoc;
#endif

    void* pRemainCtx;
    tANI_U32 rxChan;

#ifdef FEATURE_WLAN_TDLS
    tANI_U8 staType;
#endif

    // Required for indicating the frames to upper layer
    tANI_U32 beaconLength;
    tANI_U8* beaconPtr;
    tANI_U32 assocReqLength;
    tANI_U8* assocReqPtr;

    tANI_S8 rxRssi;
    tANI_U32 maxRateFlags;
#ifdef WLAN_FEATURE_AP_HT40_24G
    tpSirHT2040CoexInfoInd pSmeHT2040CoexInfoInd;
#endif
}tCsrRoamInfo;

typedef struct tagCsrFreqScanInfo
{
    tANI_U32 nStartFreq;    //in unit of MHz
    tANI_U32 nEndFreq;      //in unit of MHz
    tSirScanType scanType;
}tCsrFreqScanInfo;


typedef struct sSirSmeAssocIndToUpperLayerCnf
{
    tANI_U16             messageType; // eWNI_SME_ASSOC_CNF
    tANI_U16             length;
    tANI_U8              sessionId;
    tSirResultCodes      statusCode;
    tSirMacAddr          bssId;      // Self BSSID
    tSirMacAddr          peerMacAddr;
    tANI_U16             aid;
    tSirMacAddr          alternateBssId;
    tANI_U8              alternateChannelId;
    tANI_U8              wmmEnabledSta;   //set to true if WMM enabled STA
    tSirRSNie            rsnIE;           // RSN IE received from peer
    tSirAddie            addIE;           // Additional IE received from peer, which can be WSC and/or P2P IE
    tANI_U8              reassocReq;      //set to true if reassoc
#ifdef WLAN_FEATURE_AP_HT40_24G
    tANI_U8              HT40MHzIntoEnabledSta; //set to true if 40 MHz Intolerant enabled STA
#endif
} tSirSmeAssocIndToUpperLayerCnf, *tpSirSmeAssocIndToUpperLayerCnf;

typedef struct tagCsrSummaryStatsInfo
{
   tANI_U32 retry_cnt[4];
   tANI_U32 multiple_retry_cnt[4];
   tANI_U32 tx_frm_cnt[4];
   //tANI_U32 num_rx_frm_crc_err; same as rx_error_cnt
   //tANI_U32 num_rx_frm_crc_ok; same as rx_frm_cnt
   tANI_U32 rx_frm_cnt;
   tANI_U32 frm_dup_cnt;
   tANI_U32 fail_cnt[4];
   tANI_U32 rts_fail_cnt;
   tANI_U32 ack_fail_cnt;
   tANI_U32 rts_succ_cnt;
   tANI_U32 rx_discard_cnt;
   tANI_U32 rx_error_cnt;
   tANI_U32 tx_byte_cnt;

}tCsrSummaryStatsInfo;

typedef struct tagCsrGlobalClassAStatsInfo
{
   tANI_U32 rx_frag_cnt;
   tANI_U32 promiscuous_rx_frag_cnt;
   //tANI_U32 rx_fcs_err;
   tANI_U32 rx_input_sensitivity;
   tANI_U32 max_pwr;
   //tANI_U32 default_pwr;
   tANI_U32 sync_fail_cnt;
   tANI_U32 tx_rate;
   //mcs index for HT20 and HT40 rates
   tANI_U32  mcs_index;
   //to defferentiate between HT20 and HT40 rates;short and long guard interval
   tANI_U32  tx_rate_flags;

}tCsrGlobalClassAStatsInfo;

typedef struct tagCsrGlobalClassBStatsInfo
{
   tANI_U32 uc_rx_wep_unencrypted_frm_cnt;
   tANI_U32 uc_rx_mic_fail_cnt;
   tANI_U32 uc_tkip_icv_err;
   tANI_U32 uc_aes_ccmp_format_err;
   tANI_U32 uc_aes_ccmp_replay_cnt;
   tANI_U32 uc_aes_ccmp_decrpt_err;
   tANI_U32 uc_wep_undecryptable_cnt;
   tANI_U32 uc_wep_icv_err;
   tANI_U32 uc_rx_decrypt_succ_cnt;
   tANI_U32 uc_rx_decrypt_fail_cnt;
   tANI_U32 mcbc_rx_wep_unencrypted_frm_cnt;
   tANI_U32 mcbc_rx_mic_fail_cnt;
   tANI_U32 mcbc_tkip_icv_err;
   tANI_U32 mcbc_aes_ccmp_format_err;
   tANI_U32 mcbc_aes_ccmp_replay_cnt;
   tANI_U32 mcbc_aes_ccmp_decrpt_err;
   tANI_U32 mcbc_wep_undecryptable_cnt;
   tANI_U32 mcbc_wep_icv_err;
   tANI_U32 mcbc_rx_decrypt_succ_cnt;
   tANI_U32 mcbc_rx_decrypt_fail_cnt;

}tCsrGlobalClassBStatsInfo;

typedef struct tagCsrGlobalClassCStatsInfo
{
   tANI_U32 rx_amsdu_cnt;
   tANI_U32 rx_ampdu_cnt;
   tANI_U32 tx_20_frm_cnt;
   tANI_U32 rx_20_frm_cnt;
   tANI_U32 rx_mpdu_in_ampdu_cnt;
   tANI_U32 ampdu_delimiter_crc_err;

}tCsrGlobalClassCStatsInfo;

typedef struct tagCsrGlobalClassDStatsInfo
{
   tANI_U32 tx_uc_frm_cnt;
   tANI_U32 tx_mc_frm_cnt;
   tANI_U32 tx_bc_frm_cnt;
   tANI_U32 rx_uc_frm_cnt;
   tANI_U32 rx_mc_frm_cnt;
   tANI_U32 rx_bc_frm_cnt;
   tANI_U32 tx_uc_byte_cnt[4];
   tANI_U32 tx_mc_byte_cnt;
   tANI_U32 tx_bc_byte_cnt;
   tANI_U32 rx_uc_byte_cnt[4];
   tANI_U32 rx_mc_byte_cnt;
   tANI_U32 rx_bc_byte_cnt;
   tANI_U32 rx_byte_cnt;
   tANI_U32 num_rx_bytes_crc_ok;
   tANI_U32 rx_rate;

}tCsrGlobalClassDStatsInfo;

typedef struct tagCsrPerStaStatsInfo
{
   tANI_U32 tx_frag_cnt[4];
   tANI_U32 tx_ampdu_cnt;
   tANI_U32 tx_mpdu_in_ampdu_cnt;
} tCsrPerStaStatsInfo;

typedef struct tagCsrRoamSetKey
{
    eCsrEncryptionType encType;
    tAniKeyDirection keyDirection;    //Tx, Rx or Tx-and-Rx
    tCsrBssid peerMac;   //Peers MAC address. ALL 1's for group key
    tANI_U8 paeRole;      //0 for supplicant
    tANI_U8 keyId;  // Kye index
    tANI_U16 keyLength;  //Number of bytes containing the key in pKey
    tANI_U8 Key[CSR_MAX_KEY_LEN];
    tANI_U8 keyRsc[CSR_MAX_RSC_LEN];
} tCsrRoamSetKey;

typedef struct tagCsrRoamRemoveKey
{
    eCsrEncryptionType encType;
    tCsrBssid peerMac;   //Peers MAC address. ALL 1's for group key
    tANI_U8 keyId;  //key index
} tCsrRoamRemoveKey;

#ifdef FEATURE_WLAN_TDLS

typedef struct tagCsrLinkEstablishParams
{
    tSirMacAddr peerMac;
    tANI_U8 uapsdQueues;
    tANI_U8 qos;
    tANI_U8 maxSp;
    tANI_U8 isBufSta;
    tANI_U8 isOffChannelSupported;
    tANI_U8 isResponder;
    tANI_U8 supportedChannelsLen;
    tANI_U8 supportedChannels[SIR_MAC_MAX_SUPP_CHANNELS];
    tANI_U8 supportedOperClassesLen;
    tANI_U8 supportedOperClasses[SIR_MAC_MAX_SUPP_OPER_CLASSES];
}tCsrTdlsLinkEstablishParams;

typedef struct tagCsrTdlsSendMgmt
{
        tSirMacAddr peerMac;
        tANI_U8 frameType;
        tANI_U8 dialog;
        tANI_U16 statusCode;
        tANI_U8 responder;
        tANI_U32 peerCapability;
        tANI_U8 *buf;
        tANI_U8 len;

}tCsrTdlsSendMgmt;

#endif

typedef void * tScanResultHandle;

#define CSR_INVALID_SCANRESULT_HANDLE       (NULL)

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
typedef struct tagCsrHandoffRequest
{
    tCsrBssid bssid;
    tANI_U8 channel;
}tCsrHandoffRequest;
#endif

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
typedef struct tagCsrEseBeaconReqParams
{
    tANI_U16   measurementToken;
    tANI_U8    channel;
    tANI_U8    scanMode;
    tANI_U16   measurementDuration;
} tCsrEseBeaconReqParams, *tpCsrEseBeaconReqParams;

typedef struct tagCsrEseBeaconReq
{
    tANI_U8                numBcnReqIe;
    tCsrEseBeaconReqParams bcnReq[SIR_ESE_MAX_MEAS_IE_REQS];
} tCsrEseBeaconReq, *tpCsrEseBeaconReq;
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

struct tagCsrDelStaParams
{
    tCsrBssid peerMacAddr;
    u16 reason_code;
    u8 subtype;
};

////////////////////////////////////////////Common SCAN starts

//void *p2 -- the second context pass in for the caller
//***what if callback is called before requester gets the scanId??
typedef eHalStatus (*csrScanCompleteCallback)(tHalHandle, void *p2, tANI_U32 scanID, eCsrScanStatus status);



///////////////////////////////////////////Common Roam starts

//pContext is the pContext passed in with the roam request
//pParam is a pointer to a tCsrRoamInfo, see definition of eRoamCmdStatus and
//   eRoamCmdResult for detail valid members. It may be NULL
//roamId is to identify the callback related roam request. 0 means unsolicit
//roamStatus is a flag indicating the status of the callback
//roamResult is the result
typedef eHalStatus (*csrRoamCompleteCallback)(void *pContext, tCsrRoamInfo *pParam, tANI_U32 roamId,
                                              eRoamCmdStatus roamStatus, eCsrRoamResult roamResult);

typedef eHalStatus (*csrRoamSessionCloseCallback)(void *pContext);

/* ---------------------------------------------------------------------------
    \fn csrRoamGetNumPMKIDCache
    \brief return number of PMKID cache entries
    \return tANI_U32 - the number of PMKID cache entries
  -------------------------------------------------------------------------------*/
//tANI_U32 csrRoamGetNumPMKIDCache(tHalHandle hHal);

/* ---------------------------------------------------------------------------
    \fn csrRoamGetPMKIDCache
    \brief return PMKID cache from CSR
    \param pNum - caller allocated memory that has the space of the number of pBuf tPmkidCacheInfo as input. Upon returned, *pNum has the
    needed or actually number in tPmkidCacheInfo.
    \param pPmkidCache - Caller allocated memory that contains PMKID cache, if any, upon return
    \return eHalStatus - when fail, it usually means the buffer allocated is not big enough
  -------------------------------------------------------------------------------*/
//eHalStatus csrRoamGetPMKIDCache(tHalHandle hHal, tANI_U32 *pNum, tPmkidCacheInfo *pPmkidCache);

//pProfile - pointer to tCsrRoamProfile
#define CSR_IS_START_IBSS(pProfile) (eCSR_BSS_TYPE_START_IBSS == (pProfile)->BSSType)
#define CSR_IS_JOIN_TO_IBSS(pProfile) (eCSR_BSS_TYPE_IBSS == (pProfile)->BSSType)
#define CSR_IS_IBSS(pProfile) ( CSR_IS_START_IBSS(pProfile) || CSR_IS_JOIN_TO_IBSS(pProfile) )
#define CSR_IS_INFRASTRUCTURE(pProfile) (eCSR_BSS_TYPE_INFRASTRUCTURE == (pProfile)->BSSType)
#define CSR_IS_ANY_BSS_TYPE(pProfile) (eCSR_BSS_TYPE_ANY == (pProfile)->BSSType)
#define CSR_IS_WDS_AP( pProfile )  ( eCSR_BSS_TYPE_WDS_AP == (pProfile)->BSSType )
#define CSR_IS_WDS_STA( pProfile ) ( eCSR_BSS_TYPE_WDS_STA == (pProfile)->BSSType )
#define CSR_IS_WDS( pProfile )  ( CSR_IS_WDS_AP( pProfile ) || CSR_IS_WDS_STA( pProfile ) )
#define CSR_IS_INFRA_AP( pProfile )  ( eCSR_BSS_TYPE_INFRA_AP == (pProfile)->BSSType )

//pProfile - pointer to tCsrRoamConnectedProfile
#define CSR_IS_CONN_INFRA_AP( pProfile )  ( eCSR_BSS_TYPE_INFRA_AP == (pProfile)->BSSType )
#define CSR_IS_CONN_WDS_AP( pProfile )  ( eCSR_BSS_TYPE_WDS_AP == (pProfile)->BSSType )
#define CSR_IS_CONN_WDS_STA( pProfile ) ( eCSR_BSS_TYPE_WDS_STA == (pProfile)->BSSType )
#define CSR_IS_CONN_WDS( pProfile )  ( CSR_IS_WDS_AP( pProfile ) || CSR_IS_WDS_STA( pProfile ) )



///////////////////////////////////////////Common Roam ends


/* ---------------------------------------------------------------------------
    \fn csrSetChannels
    \brief HDD calls this function to change some global settings.
    caller must set the all fields or call csrGetConfigParam to prefill the fields.
    \param pParam - caller allocated memory
    \return eHalStatus
  -------------------------------------------------------------------------------*/

eHalStatus csrSetChannels(tHalHandle hHal,  tCsrConfigParam *pParam  );

eHalStatus csrSetRegInfo(tHalHandle hHal,  tANI_U8 *apCntryCode);


//enum to string conversion for debug output
const char * get_eRoamCmdStatus_str(eRoamCmdStatus val);
const char * get_eCsrRoamResult_str(eCsrRoamResult val);
/* ---------------------------------------------------------------------------
    \fn csrSetPhyMode
    \brief HDD calls this function to set the phyMode.
    This function must be called after CFG is downloaded and all the band/mode setting already passed into
    CSR.
    \param phyMode - indicate the phyMode needs to set to. The value has to be either 0, or some bits set.
    See eCsrPhyMode for definition
    \param eBand - specify the operational band (2.4, 5 or both)
    \param pfRestartNeeded - pointer to a caller allocated space. Upon successful return, it indicates whether
    a restart is needed to apply the change
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus csrSetPhyMode(tHalHandle hHal, tANI_U32 phyMode, eCsrBand eBand, tANI_BOOLEAN *pfRestartNeeded);

void csrDumpInit(tHalHandle hHal);


/*---------------------------------------------------------------------------
  This is the type for a link quality callback to be registered with SME
  for indications
  Once the link quality has been indicated, subsequently, link indications are
  posted each time there is a CHANGE in link quality.
  *** If there is no change in link, there will be no indication ***

  The indications may be based on one or more criteria internal to SME
  such as RSSI and PER.

  \param ind - Indication being posted
  \param pContext - any user data given at callback registration.
  \return None

---------------------------------------------------------------------------*/
typedef void (* csrRoamLinkQualityIndCallback)
             (eCsrRoamLinkQualityInd  ind, void *pContext);


/*---------------------------------------------------------------------------
  This is the type for a statistics callback to be registered with SME
  for stats reporting

  Since the client requesting for the stats already know which class/type of
  stats it asked for, the callback will carry them in the rsp buffer
  (void * stats) whose size will be same as the size of requested stats &
  will be exactly in the same order requested in the stats mask from LSB to MSB

  \param stats - stats rsp buffer sent back with the report
  \param pContext - any user data given at callback registration.
  \return None

---------------------------------------------------------------------------*/
typedef void ( *tCsrStatsCallback) (void * stats, void *pContext);

/*---------------------------------------------------------------------------
  This is the type for a rssi callback to be registered with SME
  for getting rssi

  \param rssi - rssi
  \param pContext - any user data given at callback registration.
  \return None

---------------------------------------------------------------------------*/

typedef void ( *tCsrRssiCallback) (v_S7_t rssi, tANI_U32 staId, void *pContext);


#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/*---------------------------------------------------------------------------
  This is the type for a tsm stats callback to be registered with SME
  for getting tsm stats

  \param tsmMetrics - tsmMetrics
  \param pContext - any user data given at callback registration.
  \return None

---------------------------------------------------------------------------*/

typedef void ( *tCsrTsmStatsCallback) (tAniTrafStrmMetrics tsmMetrics, tANI_U32 staId, void *pContext);
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

/*---------------------------------------------------------------------------
  This is the type for a snr callback to be registered with SME
  for getting snr

  \param snr
  \param pContext - any user data given at callback registration.
  \return None

---------------------------------------------------------------------------*/
typedef void (*tCsrSnrCallback) (v_S7_t snr, tANI_U32 staId, void *pContext);

#ifdef WLAN_FEATURE_VOWIFI_11R
eHalStatus csrRoamIssueFTPreauthReq(tHalHandle hHal, tANI_U32 sessionId, tpSirBssDescription pBssDescription);
#endif

/*---------------------------------------------------------------------------
  This is the function to change the Band configuraiton (ALL/2.4 GHZ/5 GHZ)

  \param hHal - handle to Hal context
  \param eBand - band value
  \return  eHalStatus

---------------------------------------------------------------------------*/
eHalStatus csrSetBand(tHalHandle hHal, eCsrBand eBand);

/*---------------------------------------------------------------------------
  This is the function to get the current operating band value
  \param hHal - handl to Hal context
  \return eCsrband - band value

---------------------------------------------------------------------------*/
eCsrBand csrGetCurrentBand (tHalHandle hHal);


#endif

