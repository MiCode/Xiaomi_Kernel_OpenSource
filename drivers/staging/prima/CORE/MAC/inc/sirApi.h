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




/*
 * This file sirApi.h contains definitions exported by
 * Sirius software.
 * Author:        Chandra Modumudi
 * Date:          04/16/2002
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#ifndef __SIR_API_H
#define __SIR_API_H

#include "sirTypes.h"
#include "sirMacProtDef.h"
#include "aniSystemDefs.h"
#include "sirParams.h"

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include "eseGlobal.h"
#endif

/// Maximum number of STAs allowed in the BSS
#define SIR_MAX_NUM_STA                256

/// Maximum number of Neighbors reported by STA for LB feature
#define SIR_MAX_NUM_NEIGHBOR_BSS       3

/// Maximum number of Neighbors reported by STA for LB feature
#define SIR_MAX_NUM_ALTERNATE_RADIOS   5

/// Maximum size of SCAN_RSP message
#define SIR_MAX_SCAN_RSP_MSG_LENGTH    2600

/// Start of Sirius software/Host driver message types
#define SIR_HAL_HOST_MSG_START         0x1000

/// Power save level definitions
#define SIR_MAX_POWER_SAVE          3
#define SIR_INTERMEDIATE_POWER_SAVE 4
#define SIR_NO_POWER_SAVE           5

/// Max supported channel list
#define SIR_MAX_SUPPORTED_CHANNEL_LIST      96

/// Maximum DTIM Factor
#define SIR_MAX_DTIM_FACTOR         32

#define SIR_MDIE_SIZE               3

/* Max number of channels are 165, but to access 165th element of array,
 *array of 166 is required.
 */
#define SIR_MAX_24G_5G_CHANNEL_RANGE      166
#define SIR_BCN_REPORT_MAX_BSS_DESC       4


#ifdef FEATURE_WLAN_BATCH_SCAN
#define SIR_MAX_SSID_SIZE (32)
#endif


#define SIR_NUM_11B_RATES 4   //1,2,5.5,11
#define SIR_NUM_11A_RATES 8  //6,9,12,18,24,36,48,54
#define SIR_NUM_POLARIS_RATES 3 //72,96,108
#define SIR_NUM_TITAN_RATES 26
#define SIR_NUM_TAURUS_RATES 4 //136.5, 151.7,283.5,315
#define SIR_NUM_PROP_RATES  (SIR_NUM_TITAN_RATES + SIR_NUM_TAURUS_RATES)

#define SIR_11N_PROP_RATE_136_5 (1<<28)
#define SIR_11N_PROP_RATE_151_7 (1<<29)
#define SIR_11N_PROP_RATE_283_5 (1<<30)
#define SIR_11N_PROP_RATE_315     (1<<31)
#define SIR_11N_PROP_RATE_BITMAP 0x80000000 //only 315MBPS rate is supported today
//Taurus is going to support 26 Titan Rates(no ESF/concat Rates will be supported)
//First 26 bits are reserved for Titan and last 4 bits for Taurus, 2(27 and 28) bits are reserved.
//#define SIR_TITAN_PROP_RATE_BITMAP 0x03FFFFFF
//Disable all Titan rates
#define SIR_TITAN_PROP_RATE_BITMAP 0
#define SIR_CONVERT_2_U32_BITMAP(nRates) ((nRates + 31)/32)

/* #tANI_U32's needed for a bitmap representation for all prop rates */
#define SIR_NUM_U32_MAP_RATES    SIR_CONVERT_2_U32_BITMAP(SIR_NUM_PROP_RATES)


#define SIR_PM_SLEEP_MODE   0
#define SIR_PM_ACTIVE_MODE        1

// Used by various modules to load ALL CFG's
#define ANI_IGNORE_CFG_ID 0xFFFF

//hidden SSID options
#define SIR_SCAN_NO_HIDDEN_SSID                      0
#define SIR_SCAN_HIDDEN_SSID_PE_DECISION             1
#define SIR_SCAN_HIDDEN_SSID                         2

#define SIR_MAC_ADDR_LEN        6
#define SIR_IPV4_ADDR_LEN       4

typedef tANI_U8 tSirIpv4Addr[SIR_IPV4_ADDR_LEN];

#define SIR_VERSION_STRING_LEN 64
typedef tANI_U8 tSirVersionString[SIR_VERSION_STRING_LEN];

/* Periodic Tx pattern offload feature */
#define PERIODIC_TX_PTRN_MAX_SIZE 1536
#define MAXNUM_PERIODIC_TX_PTRNS 6


#ifdef WLAN_FEATURE_EXTSCAN

#define WLAN_EXTSCAN_MAX_CHANNELS                 16
#define WLAN_EXTSCAN_MAX_BUCKETS                  16
#define WLAN_EXTSCAN_MAX_HOTLIST_APS              128
#define WLAN_EXTSCAN_MAX_SIGNIFICANT_CHANGE_APS   64
#define WLAN_EXTSCAN_MAX_RSSI_SAMPLE_SIZE     8
#endif /* WLAN_FEATURE_EXTSCAN */

#define WLAN_DISA_MAX_PAYLOAD_SIZE                1600

enum eSirHostMsgTypes
{
    SIR_HAL_APP_SETUP_NTF = SIR_HAL_HOST_MSG_START,
    SIR_HAL_INITIAL_CAL_FAILED_NTF,
    SIR_HAL_NIC_OPER_NTF,
    SIR_HAL_INIT_START_REQ,
    SIR_HAL_SHUTDOWN_REQ,
    SIR_HAL_SHUTDOWN_CNF,
    SIR_HAL_RESET_REQ,
    SIR_HAL_RADIO_ON_OFF_IND,    
    SIR_HAL_RESET_CNF,
    SIR_WRITE_TO_TD,
    SIR_HAL_HDD_ADDBA_REQ, // MAC -> HDD
    SIR_HAL_HDD_ADDBA_RSP, // HDD -> HAL        
    SIR_HAL_DELETEBA_IND, // MAC -> HDD
    SIR_HAL_BA_FAIL_IND, // HDD -> MAC
    SIR_TL_HAL_FLUSH_AC_REQ, 
    SIR_HAL_TL_FLUSH_AC_RSP
};



/**
 * Module ID definitions.
 */
enum {
    SIR_BOOT_MODULE_ID = 1,
    SIR_HAL_MODULE_ID  = 0x10,
    SIR_CFG_MODULE_ID = 0x12,
    SIR_LIM_MODULE_ID,
    SIR_ARQ_MODULE_ID,
    SIR_SCH_MODULE_ID,
    SIR_PMM_MODULE_ID,
    SIR_MNT_MODULE_ID,
    SIR_DBG_MODULE_ID,
    SIR_DPH_MODULE_ID,
    SIR_SYS_MODULE_ID,
    SIR_SMS_MODULE_ID,

    SIR_PHY_MODULE_ID = 0x20,


    // Add any modules above this line
    SIR_DVT_MODULE_ID
};

#define SIR_WDA_MODULE_ID SIR_HAL_MODULE_ID

/**
 * First and last module definition for logging utility
 *
 * NOTE:  The following definitions need to be updated if
 *        the above list is changed.
 */
#define SIR_FIRST_MODULE_ID     SIR_HAL_MODULE_ID
#define SIR_LAST_MODULE_ID      SIR_DVT_MODULE_ID


// Type declarations used by Firmware and Host software

// Scan type enum used in scan request
typedef enum eSirScanType
{
    eSIR_PASSIVE_SCAN,
    eSIR_ACTIVE_SCAN,
    eSIR_BEACON_TABLE,
} tSirScanType;

typedef enum eSirResultCodes
{
    eSIR_SME_SUCCESS,

    eSIR_EOF_SOF_EXCEPTION,
    eSIR_BMU_EXCEPTION,
    eSIR_LOW_PDU_EXCEPTION,
    eSIR_USER_TRIG_RESET,
    eSIR_LOGP_EXCEPTION,
    eSIR_CP_EXCEPTION,
    eSIR_STOP_BSS,
    eSIR_AHB_HANG_EXCEPTION,
    eSIR_DPU_EXCEPTION,
    eSIR_RPE_EXCEPTION,
    eSIR_TPE_EXCEPTION,
    eSIR_DXE_EXCEPTION,
    eSIR_RXP_EXCEPTION,
    eSIR_MCPU_EXCEPTION,
    eSIR_MCU_EXCEPTION,
    eSIR_MTU_EXCEPTION,
    eSIR_MIF_EXCEPTION,
    eSIR_FW_EXCEPTION,
    eSIR_PS_MUTEX_READ_EXCEPTION,
    eSIR_PHY_HANG_EXCEPTION,
    eSIR_MAILBOX_SANITY_CHK_FAILED,
    eSIR_RADIO_HW_SWITCH_STATUS_IS_OFF, // Only where this switch is present
    eSIR_CFB_FLAG_STUCK_EXCEPTION,

    eSIR_SME_BASIC_RATES_NOT_SUPPORTED_STATUS=30,

    eSIR_SME_INVALID_PARAMETERS=500,
    eSIR_SME_UNEXPECTED_REQ_RESULT_CODE,
    eSIR_SME_RESOURCES_UNAVAILABLE,
    eSIR_SME_SCAN_FAILED,   // Unable to find a BssDescription
                            // matching requested scan criteria
    eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED,
    eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE,
    eSIR_SME_REFUSED,
    eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA,
    eSIR_SME_JOIN_TIMEOUT_RESULT_CODE,
    eSIR_SME_AUTH_TIMEOUT_RESULT_CODE,
    eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE,
    eSIR_SME_REASSOC_TIMEOUT_RESULT_CODE,
    eSIR_SME_MAX_NUM_OF_PRE_AUTH_REACHED,
    eSIR_SME_AUTH_REFUSED,
    eSIR_SME_INVALID_WEP_DEFAULT_KEY,
    eSIR_SME_NO_KEY_MAPPING_KEY_FOR_PEER,
    eSIR_SME_ASSOC_REFUSED,
    eSIR_SME_REASSOC_REFUSED,
    eSIR_SME_DEAUTH_WHILE_JOIN, //Received Deauth while joining or pre-auhtentication.
    eSIR_SME_DISASSOC_WHILE_JOIN, //Received Disassociation while joining.
    eSIR_SME_DEAUTH_WHILE_REASSOC, //Received Deauth while ReAssociate.
    eSIR_SME_DISASSOC_WHILE_REASSOC, //Received Disassociation while ReAssociate
    eSIR_SME_STA_NOT_AUTHENTICATED,
    eSIR_SME_STA_NOT_ASSOCIATED,
    eSIR_SME_STA_DISASSOCIATED,
    eSIR_SME_ALREADY_JOINED_A_BSS,
    eSIR_ULA_COMPLETED,
    eSIR_ULA_FAILURE,
    eSIR_SME_LINK_ESTABLISHED,
    eSIR_SME_UNABLE_TO_PERFORM_MEASUREMENTS,
    eSIR_SME_UNABLE_TO_PERFORM_DFS,
    eSIR_SME_DFS_FAILED,
    eSIR_SME_TRANSFER_STA, // To be used when STA need to be LB'ed
    eSIR_SME_INVALID_LINK_TEST_PARAMETERS,// Given in LINK_TEST_START_RSP
    eSIR_SME_LINK_TEST_MAX_EXCEEDED,    // Given in LINK_TEST_START_RSP
    eSIR_SME_UNSUPPORTED_RATE,          // Given in LINK_TEST_RSP if peer does
                                        // support requested rate in
                                        // LINK_TEST_REQ
    eSIR_SME_LINK_TEST_TIMEOUT,         // Given in LINK_TEST_IND if peer does
                                        // not respond before next test packet
                                        // is sent
    eSIR_SME_LINK_TEST_COMPLETE,        // Given in LINK_TEST_IND at the end
                                        // of link test
    eSIR_SME_LINK_TEST_INVALID_STATE,   // Given in LINK_TEST_START_RSP
    eSIR_SME_LINK_TEST_TERMINATE,       // Given in LINK_TEST_START_RSP
    eSIR_SME_LINK_TEST_INVALID_ADDRESS, // Given in LINK_TEST_STOP_RSP
    eSIR_SME_POLARIS_RESET,             // Given in SME_STOP_BSS_REQ
    eSIR_SME_SETCONTEXT_FAILED,         // Given in SME_SETCONTEXT_REQ when
                                        // unable to plumb down keys
    eSIR_SME_BSS_RESTART,               // Given in SME_STOP_BSS_REQ

    eSIR_SME_MORE_SCAN_RESULTS_FOLLOW,  // Given in SME_SCAN_RSP message
                                        // that more SME_SCAN_RSP
                                        // messages are following.
                                        // SME_SCAN_RSP message with
                                        // eSIR_SME_SUCCESS status
                                        // code is the last one.
    eSIR_SME_INVALID_ASSOC_RSP_RXED,    // Sent in SME_JOIN/REASSOC_RSP
                                        // messages upon receiving
                                        // invalid Re/Assoc Rsp frame.
    eSIR_SME_MIC_COUNTER_MEASURES,      // STOP BSS triggered by MIC failures: MAC software to disassoc all stations
                                        // with MIC_FAILURE reason code and perform the stop bss operation
    eSIR_SME_ADDTS_RSP_TIMEOUT,         // didn't get response from peer within
                                        // timeout interval
    eSIR_SME_ADDTS_RSP_FAILED,          // didn't get success response from HAL
    eSIR_SME_RECEIVED,
    // TBA - TSPEC related Result Codes

    eSIR_SME_CHANNEL_SWITCH_FAIL,        // failed to send out Channel Switch Action Frame
    eSIR_SME_INVALID_STA_ROLE,
    eSIR_SME_INVALID_STATE,
#ifdef GEN4_SCAN
    eSIR_SME_CHANNEL_SWITCH_DISABLED,    // either 11h is disabled or channelSwitch is currently active
    eSIR_SME_HAL_SCAN_INIT_FAILED,       // SIR_HAL_SIR_HAL_INIT_SCAN_RSP returned failed status
    eSIR_SME_HAL_SCAN_START_FAILED,      // SIR_HAL_START_SCAN_RSP returned failed status
    eSIR_SME_HAL_SCAN_END_FAILED,        // SIR_HAL_END_SCAN_RSP returned failed status
    eSIR_SME_HAL_SCAN_FINISH_FAILED,     // SIR_HAL_FINISH_SCAN_RSP returned failed status
    eSIR_SME_HAL_SEND_MESSAGE_FAIL,      // Failed to send a message to HAL
#else // GEN4_SCAN
    eSIR_SME_CHANNEL_SWITCH_DISABLED,    // either 11h is disabled or channelSwitch is currently active
    eSIR_SME_HAL_SEND_MESSAGE_FAIL,      // Failed to send a message to HAL
#endif // GEN4_SCAN
#ifdef FEATURE_OEM_DATA_SUPPORT
    eSIR_SME_HAL_OEM_DATA_REQ_START_FAILED,
#endif
    eSIR_SME_STOP_BSS_FAILURE,           // Failed to stop the bss
    eSIR_SME_STA_ASSOCIATED,
    eSIR_SME_INVALID_PMM_STATE,
    eSIR_SME_CANNOT_ENTER_IMPS,
    eSIR_SME_IMPS_REQ_FAILED,
    eSIR_SME_BMPS_REQ_FAILED,
    eSIR_SME_BMPS_REQ_REJECT,
    eSIR_SME_UAPSD_REQ_FAILED,
    eSIR_SME_WOWL_ENTER_REQ_FAILED,
    eSIR_SME_WOWL_EXIT_REQ_FAILED,
#if defined WLAN_FEATURE_VOWIFI_11R
    eSIR_SME_FT_REASSOC_TIMEOUT_FAILURE,
    eSIR_SME_FT_REASSOC_FAILURE,
#endif
    eSIR_SME_SEND_ACTION_FAIL,
#ifdef WLAN_FEATURE_PACKET_FILTERING
    eSIR_SME_PC_FILTER_MATCH_COUNT_REQ_FAILED,
#endif // WLAN_FEATURE_PACKET_FILTERING
    
#ifdef WLAN_FEATURE_GTK_OFFLOAD
    eSIR_SME_GTK_OFFLOAD_GETINFO_REQ_FAILED,
#endif // WLAN_FEATURE_GTK_OFFLOAD
    eSIR_SME_DEAUTH_STATUS,
    eSIR_DONOT_USE_RESULT_CODE = SIR_MAX_ENUM_SIZE    
} tSirResultCodes;

/* each station added has a rate mode which specifies the sta attributes */
typedef enum eStaRateMode {
    eSTA_TAURUS = 0,
    eSTA_TITAN,
    eSTA_POLARIS,
    eSTA_11b,
    eSTA_11bg,
    eSTA_11a,
    eSTA_11n,
#ifdef WLAN_FEATURE_11AC
    eSTA_11ac,
#endif
    eSTA_INVALID_RATE_MODE
} tStaRateMode, *tpStaRateMode;

//although in tSirSupportedRates each IE is 16bit but PE only passes IEs in 8 bits with MSB=1 for basic rates.
//change the mask for bit0-7 only so HAL gets correct basic rates for setting response rates.
#define IERATE_BASICRATE_MASK     0x80
#define IERATE_RATE_MASK          0x7f
#define IERATE_IS_BASICRATE(x)   ((x) & IERATE_BASICRATE_MASK)
#define ANIENHANCED_TAURUS_RATEMAP_BITOFFSET_START  28

const char * lim_BssTypetoString(const v_U8_t bssType);
const char * lim_ScanTypetoString(const v_U8_t scanType);
const char * lim_BackgroundScanModetoString(const v_U8_t mode);
typedef struct sSirSupportedRates {
    /*
    * For Self STA Entry: this represents Self Mode.
    * For Peer Stations, this represents the mode of the peer.
    * On Station:
    * --this mode is updated when PE adds the Self Entry.
    * -- OR when PE sends 'ADD_BSS' message and station context in BSS is used to indicate the mode of the AP.
    * ON AP:
    * -- this mode is updated when PE sends 'ADD_BSS' and Sta entry for that BSS is used
    *     to indicate the self mode of the AP.
    * -- OR when a station is associated, PE sends 'ADD_STA' message with this mode updated.
    */

    tStaRateMode        opRateMode;
    // 11b, 11a and aniLegacyRates are IE rates which gives rate in unit of 500Kbps
    tANI_U16             llbRates[SIR_NUM_11B_RATES];
    tANI_U16             llaRates[SIR_NUM_11A_RATES];
    tANI_U16             aniLegacyRates[SIR_NUM_POLARIS_RATES];

    //Taurus only supports 26 Titan Rates(no ESF/concat Rates will be supported)
    //First 26 bits are reserved for those Titan rates and
    //the last 4 bits(bit28-31) for Taurus, 2(bit26-27) bits are reserved.
    tANI_U32             aniEnhancedRateBitmap; //Titan and Taurus Rates

    /*
    * 0-76 bits used, remaining reserved
    * bits 0-15 and 32 should be set.
    */
    tANI_U8 supportedMCSSet[SIR_MAC_MAX_SUPPORTED_MCS_SET];

    /*
     * RX Highest Supported Data Rate defines the highest data
     * rate that the STA is able to receive, in unites of 1Mbps.
     * This value is derived from "Supported MCS Set field" inside
     * the HT capability element.
     */
    tANI_U16 rxHighestDataRate;

#ifdef WLAN_FEATURE_11AC
   /*Indicates the Maximum MCS that can be received for each number
        of spacial streams */
    tANI_U16 vhtRxMCSMap;
   /*Indicate the highest VHT data rate that the STA is able to receive*/
    tANI_U16 vhtRxHighestDataRate;
   /*Indicates the Maximum MCS that can be transmitted for each number
        of spacial streams */
    tANI_U16 vhtTxMCSMap;
   /*Indicate the highest VHT data rate that the STA is able to transmit*/
    tANI_U16 vhtTxHighestDataRate;
#endif
} tSirSupportedRates, *tpSirSupportedRates;


typedef enum eSirRFBand
{
    SIR_BAND_UNKNOWN,
    SIR_BAND_2_4_GHZ,
    SIR_BAND_5_GHZ,
} tSirRFBand;


/*
* Specifies which beacons are to be indicated upto the host driver when
* Station is in power save mode.
*/
typedef enum eBeaconForwarding
{
    ePM_BEACON_FWD_NTH,
    ePM_BEACON_FWD_TIM,
    ePM_BEACON_FWD_DTIM,
    ePM_BEACON_FWD_NONE
} tBeaconForwarding;


typedef struct sSirRemainOnChnReq
{
    tANI_U16 messageType;
    tANI_U16 length;
    tANI_U8 sessionId;
    tSirMacAddr selfMacAddr;
    tANI_U8  chnNum;
    tANI_U8  phyMode;
    tANI_U32 duration;
    tANI_U8  isProbeRequestAllowed;
    tANI_U8  probeRspIe[1];
}tSirRemainOnChnReq, *tpSirRemainOnChnReq;

/* Structure for vendor specific IE of debug marker frame
   to debug remain on channel issues */
typedef struct publicVendorSpecific
{
    tANI_U8 category;
    tANI_U8 elementid;
    tANI_U8 length;
} publicVendorSpecific;

typedef struct sSirRegisterMgmtFrame
{
    tANI_U16 messageType;
    tANI_U16 length;
    tANI_U8 sessionId;
    tANI_BOOLEAN registerFrame;
    tANI_U16 frameType;
    tANI_U16 matchLen;
    tANI_U8  matchData[1];
}tSirRegisterMgmtFrame, *tpSirRegisterMgmtFrame;

//
// Identifies the neighbor BSS' that was(were) detected
// by an STA and reported to the AP
//
typedef struct sAniTitanCBNeighborInfo
{
  // A BSS was found on the Primary
  tANI_U8 cbBssFoundPri;

  // A BSS was found on the adjacent Upper Secondary
  tANI_U8 cbBssFoundSecUp;

  // A BSS was found on the adjacent Lower Secondary
  tANI_U8 cbBssFoundSecDown;

} tAniTitanCBNeighborInfo, *tpAniTitanCBNeighborInfo;

/// Generic type for sending a response message
/// with result code to host software
typedef struct sSirSmeRsp
{
    tANI_U16             messageType; // eWNI_SME_*_RSP
    tANI_U16             length;
    tANI_U8              sessionId;  // To support BT-AMP
    tANI_U16             transactionId;   // To support BT-AMP
    tSirResultCodes statusCode;
} tSirSmeRsp, *tpSirSmeRsp;

/// Definition for kick starting Firmware on STA
typedef struct sSirSmeStartReq
{
    tANI_U16   messageType;      // eWNI_SME_START_REQ
    tANI_U16   length;    
    tANI_U8      sessionId;      //Added for BT-AMP Support
    tANI_U16     transcationId;  //Added for BT-AMP Support
    tSirMacAddr  bssId;          //Added For BT-AMP Support   
    tANI_U32   roamingAtPolaris;
    tANI_U32   sendNewBssInd;
} tSirSmeStartReq, *tpSirSmeStartReq;

/// Definition for indicating all modules ready on STA
typedef struct sSirSmeReadyReq
{
    tANI_U16   messageType; // eWNI_SME_SYS_READY_IND
    tANI_U16   length;
    tANI_U16   transactionId;     
} tSirSmeReadyReq, *tpSirSmeReadyReq;

/// Definition for response message to previously issued start request
typedef struct sSirSmeStartRsp
{
    tANI_U16             messageType; // eWNI_SME_START_RSP
    tANI_U16             length;
    tSirResultCodes statusCode;
    tANI_U16             transactionId;     
} tSirSmeStartRsp, *tpSirSmeStartRsp;


/// Definition for Load structure
typedef struct sSirLoad
{
    tANI_U16             numStas;
    tANI_U16             channelUtilization;
} tSirLoad, *tpSirLoad;

/// BSS type enum used in while scanning/joining etc
typedef enum eSirBssType
{
    eSIR_INFRASTRUCTURE_MODE,
    eSIR_INFRA_AP_MODE,                    //Added for softAP support
    eSIR_IBSS_MODE,
    eSIR_BTAMP_STA_MODE,                     //Added for BT-AMP support
    eSIR_BTAMP_AP_MODE,                     //Added for BT-AMP support
    eSIR_AUTO_MODE,
    eSIR_DONOT_USE_BSS_TYPE = SIR_MAX_ENUM_SIZE
} tSirBssType;

/// Definition for WDS Information
typedef struct sSirWdsInfo
{
    tANI_U16                wdsLength;
    tANI_U8                 wdsBytes[ANI_WDS_INFO_MAX_LENGTH];
} tSirWdsInfo, *tpSirWdsInfo;

/// Power Capability info used in 11H
typedef struct sSirMacPowerCapInfo
{
    tANI_U8              minTxPower;
    tANI_U8              maxTxPower;
} tSirMacPowerCapInfo, *tpSirMacPowerCapInfo;

/// Supported Channel info used in 11H
typedef struct sSirSupChnl
{
    tANI_U8              numChnl;
    tANI_U8              channelList[SIR_MAX_SUPPORTED_CHANNEL_LIST];
} tSirSupChnl, *tpSirSupChnl;

typedef enum eSirNwType
{
    eSIR_11A_NW_TYPE,
    eSIR_11B_NW_TYPE,
    eSIR_11G_NW_TYPE,
    eSIR_11N_NW_TYPE,
#ifdef WLAN_FEATURE_11AC
    eSIR_11AC_NW_TYPE,
#endif
    eSIR_DONOT_USE_NW_TYPE = SIR_MAX_ENUM_SIZE
} tSirNwType;

/// Definition for new iBss peer info
typedef struct sSirNewIbssPeerInfo
{
    tSirMacAddr    peerAddr;
    tANI_U16            aid;
} tSirNewIbssPeerInfo, *tpSirNewIbssPeerInfo;

/// Definition for Alternate BSS info
typedef struct sSirAlternateRadioInfo
{
    tSirMacAddr    bssId;
    tANI_U8             channelId;
} tSirAlternateRadioInfo, *tpSirAlternateRadioInfo;

/// Definition for Alternate BSS list
typedef struct sSirAlternateRadioList
{
    tANI_U8                       numBss;
    tSirAlternateRadioInfo   alternateRadio[1];
} tSirAlternateRadioList, *tpSirAlternateRadioList;

/// Definition for kick starting BSS
/// ---> MAC
/**
 * Usage of ssId, numSSID & ssIdList:
 * ---------------------------------
 * 1. ssId.length of zero indicates that Broadcast/Suppress SSID
 *    feature is enabled.
 * 2. If ssId.length is zero, MAC SW will advertise NULL SSID
 *    and interpret the SSID list from numSSID & ssIdList.
 * 3. If ssId.length is non-zero, MAC SW will advertise the SSID
 *    specified in the ssId field and it is expected that
 *    application will set numSSID to one (only one SSID present
 *    in the list) and SSID in the list is same as ssId field.
 * 4. Application will always set numSSID >= 1.
 */
//*****NOTE: Please make sure all codes are updated if inserting field into this structure..**********
typedef struct sSirSmeStartBssReq
{
    tANI_U16                messageType;       // eWNI_SME_START_BSS_REQ
    tANI_U16                length;
    tANI_U8                 sessionId;       //Added for BT-AMP Support
    tANI_U16                transactionId;   //Added for BT-AMP Support
    tSirMacAddr             bssId;           //Added for BT-AMP Support
    tSirMacAddr             selfMacAddr;     //Added for BT-AMP Support
    tANI_U16                beaconInterval;  //Added for BT-AMP Support
    tANI_U8                 dot11mode;
    tSirBssType             bssType;
    tSirMacSSid             ssId;
    tANI_U8                 channelId;
    ePhyChanBondState       cbMode;
    
    tANI_U8                 privacy;
    tANI_U8                 apUapsdEnable;
    tANI_U8                 ssidHidden;
    tANI_BOOLEAN            fwdWPSPBCProbeReq;
    tANI_BOOLEAN            protEnabled;
    tANI_BOOLEAN            obssProtEnabled;
    tANI_U16                ht_capab;
    tAniAuthType            authType;
    tANI_U32                dtimPeriod;
    tANI_U8                 wps_state;
    tANI_U8                 isCoalesingInIBSSAllowed; //Coalesing on/off knob
    tVOS_CON_MODE           bssPersona;

    tANI_U8                 txLdpcIniFeatureEnabled;

    tSirRSNie               rsnIE;             // RSN IE to be sent in
                                               // Beacon and Probe
                                               // Response frames
    tSirNwType              nwType;            // Indicates 11a/b/g
    tSirMacRateSet          operationalRateSet;// Has 11a or 11b rates
    tSirMacRateSet          extendedRateSet;    // Has 11g rates

#ifdef WLAN_FEATURE_11W
    tANI_BOOLEAN            pmfCapable;
    tANI_BOOLEAN            pmfRequired;
#endif

} tSirSmeStartBssReq, *tpSirSmeStartBssReq;

#define GET_IE_LEN_IN_BSS(lenInBss) ( lenInBss + sizeof(lenInBss) - \
              ((uintptr_t)OFFSET_OF( tSirBssDescription, ieFields)))

#define WSCIE_PROBE_RSP_LEN (317 + 2)

typedef struct sSirBssDescription
{
    //offset of the ieFields from bssId.
    tANI_U16             length;
    tSirMacAddr          bssId;
    v_TIME_t             scanSysTimeMsec;
    tANI_U32             timeStamp[2];
    tANI_U16             beaconInterval;
    tANI_U16             capabilityInfo;
    tSirNwType           nwType; // Indicates 11a/b/g
    tANI_U8              aniIndicator;
    tANI_S8              rssi;
    tANI_S8              sinr;
    //channelId what peer sent in beacon/probersp.
    tANI_U8              channelId;
    //channelId on which we are parked at.
    //used only in scan case.
    tANI_U8              channelIdSelf;
    tANI_U8              sSirBssDescriptionRsvd[3];
    tANI_TIMESTAMP nReceivedTime;     //base on a tick count. It is a time stamp, not a relative time.
#if defined WLAN_FEATURE_VOWIFI
    tANI_U32       parentTSF;
    tANI_U32       startTSF[2];
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
    tANI_U8              mdiePresent;
    tANI_U8              mdie[SIR_MDIE_SIZE];                // MDIE for 11r, picked from the beacons
#endif
#ifdef FEATURE_WLAN_ESE
    tANI_U16             QBSSLoad_present;
    tANI_U16             QBSSLoad_avail; 
#endif
    // Please keep the structure 4 bytes aligned above the ieFields

    tANI_U8              fProbeRsp; //whether it is from a probe rsp
    tANI_U8              reservedPadding1;
    tANI_U8              reservedPadding2;
    tANI_U8              reservedPadding3;
    tANI_U32             WscIeLen;
    tANI_U8              WscIeProbeRsp[WSCIE_PROBE_RSP_LEN];
    tANI_U8              reservedPadding4;
    
    tANI_U32             ieFields[1];
} tSirBssDescription, *tpSirBssDescription;

/// Definition for response message to previously
/// issued start BSS request
/// MAC --->
typedef struct sSirSmeStartBssRsp
{
    tANI_U16            messageType; // eWNI_SME_START_BSS_RSP
    tANI_U16            length;
    tANI_U8             sessionId;
    tANI_U16            transactionId;//transaction ID for cmd
    tSirResultCodes     statusCode;
    tSirBssType         bssType;//Add new type for WDS mode
    tANI_U16            beaconInterval;//Beacon Interval for both type
    tANI_U32            staId;//Staion ID for Self  
    tSirBssDescription  bssDescription;//Peer BSS description
} tSirSmeStartBssRsp, *tpSirSmeStartBssRsp;


typedef struct sSirChannelList
{
    tANI_U8          numChannels;
    tANI_U8          channelNumber[SIR_ESE_MAX_MEAS_IE_REQS];
} tSirChannelList, *tpSirChannelList;

typedef struct sSirDFSChannelList
{
    tANI_U32         timeStamp[SIR_MAX_24G_5G_CHANNEL_RANGE];

} tSirDFSChannelList, *tpSirDFSChannelList;

#ifdef FEATURE_WLAN_ESE
typedef struct sTspecInfo {
    tANI_U8         valid;
    tSirMacTspecIE  tspec;
} tTspecInfo;

#define SIR_ESE_MAX_TSPEC_IES   4
typedef struct sESETspecTspecInfo {
    tANI_U8 numTspecs;
    tTspecInfo tspec[SIR_ESE_MAX_TSPEC_IES];
} tESETspecInfo;
#endif


/// Definition for Radar Info
typedef struct sSirRadarInfo
{
    tANI_U8          channelNumber;
    tANI_U16         radarPulseWidth; // in usecond
    tANI_U16         numRadarPulse;
} tSirRadarInfo, *tpSirRadarInfo;

#define SIR_RADAR_INFO_SIZE                (sizeof(tANI_U8) + 2 *sizeof(tANI_U16))

/// Two Background Scan mode
typedef enum eSirBackgroundScanMode
{
    eSIR_AGGRESSIVE_BACKGROUND_SCAN = 0,
    eSIR_NORMAL_BACKGROUND_SCAN = 1,
    eSIR_ROAMING_SCAN = 2,
} tSirBackgroundScanMode;
typedef enum eSirLinkTrafficCheck
{
    eSIR_DONT_CHECK_LINK_TRAFFIC_BEFORE_SCAN = 0,
    eSIR_CHECK_LINK_TRAFFIC_BEFORE_SCAN = 1,
    eSIR_CHECK_ROAMING_SCAN = 2,
} tSirLinkTrafficCheck;

#define SIR_BG_SCAN_RETURN_CACHED_RESULTS              0x0
#define SIR_BG_SCAN_PURGE_RESUTLS                      0x80
#define SIR_BG_SCAN_RETURN_FRESH_RESULTS               0x01
#define SIR_SCAN_MAX_NUM_SSID                          0x09 
#define SIR_BG_SCAN_RETURN_LFR_CACHED_RESULTS          0x02
#define SIR_BG_SCAN_PURGE_LFR_RESULTS                  0x40

/// Definition for scan request
typedef struct sSirSmeScanReq
{
    tANI_U16        messageType; // eWNI_SME_SCAN_REQ
    tANI_U16        length;
    tANI_U8         sessionId;         // Session ID
    tANI_U16        transactionId;     // Transaction ID for cmd
    tSirMacAddr     bssId;
    tSirMacSSid     ssId[SIR_SCAN_MAX_NUM_SSID];
    tSirMacAddr     selfMacAddr; //Added For BT-AMP Support
    tSirBssType     bssType;
    tANI_U8         dot11mode;
    tSirScanType    scanType;
    /**
     * minChannelTime. Not used if scanType is passive.
     * 0x0 - Dont Use min channel timer. Only max channel timeout will used.
     *       11k measurements set this to zero to user only single duration for scan.
     * <valid timeout> - Timeout value used for min channel timeout.
     */
    tANI_U32        minChannelTime;
    /**
     * maxChannelTime.
     * 0x0 - Invalid. In case of active scan.
     * In case of passive scan, MAX( maxChannelTime, WNI_CFG_PASSIVE_MAXIMUM_CHANNEL_TIME) is used. 
     *
     */
    tANI_U32        maxChannelTime;
    /**
     * returnAfterFirstMatch can take following values:
     * 0x00 - Return SCAN_RSP message after complete channel scan
     * 0x01 -  Return SCAN_RSP message after collecting BSS description
     *        that matches scan criteria.
     * 0xC0 - Return after collecting first 11d IE from 2.4 GHz &
     *        5 GHz band channels
     * 0x80 - Return after collecting first 11d IE from 5 GHz band
     *        channels
     * 0x40 - Return after collecting first 11d IE from 2.4 GHz
     *        band channels
     *
     * Values of 0xC0, 0x80 & 0x40 are to be used by
     * Roaming/application when 11d is enabled.
     */
    tANI_U32 minChannelTimeBtc;    //in units of milliseconds
    tANI_U32 maxChannelTimeBtc;    //in units of milliseconds
    tANI_U8              returnAfterFirstMatch;

    /**
     * returnUniqueResults can take following values:
     * 0 - Collect & report all received BSS descriptions from same BSS.
     * 1 - Collect & report unique BSS description from same BSS.
     */
    tANI_U8              returnUniqueResults;

    /**
     * returnFreshResults can take following values:
     * 0x00 - Return background scan results.
     * 0x80 - Return & purge background scan results
     * 0x01 - Trigger fresh scan instead of returning background scan
     *        results.
     * 0x81 - Trigger fresh scan instead of returning background scan
     *        results and purge background scan results.
     */
    tANI_U8              returnFreshResults;

    /*  backgroundScanMode can take following values:
     *  0x0 - agressive scan
     *  0x1 - normal scan where HAL will check for link traffic 
     *        prior to proceeding with the scan
     */
    tSirBackgroundScanMode   backgroundScanMode;

    tANI_U8              hiddenSsid;

    /* Number of SSIDs to scan */
    tANI_U8             numSsid;
    
    //channelList has to be the last member of this structure. Check tSirChannelList for the reason.
    /* This MUST be the last field of the structure */
    
 
    tANI_BOOLEAN         p2pSearch;
    tANI_U16             uIEFieldLen;
    tANI_U16             uIEFieldOffset;

    //channelList MUST be the last field of this structure
    tSirChannelList channelList;
    /*-----------------------------
      tSirSmeScanReq....
      -----------------------------
      uIEFiledLen 
      -----------------------------
      uIEFiledOffset               ----+
      -----------------------------    |
      channelList.numChannels          |
      -----------------------------    |
      ... variable size up to          |
      channelNumber[numChannels-1]     |
      This can be zero, if             |
      numChannel is zero.              |
      ----------------------------- <--+
      ... variable size uIEFiled 
      up to uIEFieldLen (can be 0)
      -----------------------------*/
} tSirSmeScanReq, *tpSirSmeScanReq;

typedef struct sSirSmeScanAbortReq
{
    tANI_U16        type;
    tANI_U16        msgLen;
    tANI_U8         sessionId;
} tSirSmeScanAbortReq, *tpSirSmeScanAbortReq;

typedef struct sSirSmeScanChanReq
{
    tANI_U16        type;
    tANI_U16        msgLen;
    tANI_U8         sessionId;
    tANI_U16        transcationId;
} tSirSmeGetScanChanReq, *tpSirSmeGetScanChanReq;

#ifdef FEATURE_OEM_DATA_SUPPORT

#ifndef OEM_DATA_REQ_SIZE
#define OEM_DATA_REQ_SIZE 134
#endif
#ifndef OEM_DATA_RSP_SIZE
#define OEM_DATA_RSP_SIZE 1968
#endif

typedef struct sSirOemDataReq
{
    tANI_U16              messageType; //eWNI_SME_OEM_DATA_REQ
    tANI_U16              messageLen;
    tSirMacAddr           selfMacAddr;
    tANI_U8               oemDataReq[OEM_DATA_REQ_SIZE];
} tSirOemDataReq, *tpSirOemDataReq;

typedef struct sSirOemDataRsp
{
    tANI_U16             messageType;
    tANI_U16             length;
    tANI_U8              oemDataRsp[OEM_DATA_RSP_SIZE];
} tSirOemDataRsp, *tpSirOemDataRsp;
    
#endif //FEATURE_OEM_DATA_SUPPORT

/// Definition for response message to previously issued scan request
typedef struct sSirSmeScanRsp
{
    tANI_U16           messageType; // eWNI_SME_SCAN_RSP
    tANI_U16           length;
    tANI_U8            sessionId;     
    tSirResultCodes    statusCode;
    tANI_U16           transcationId; 
    tSirBssDescription bssDescription[1];
} tSirSmeScanRsp, *tpSirSmeScanRsp;

/// Sme Req message to set the Background Scan mode
typedef struct sSirSmeBackgroundScanModeReq
{
    tANI_U16                      messageType; // eWNI_SME_BACKGROUND_SCAN_MODE_REQ
    tANI_U16                      length;
    tSirBackgroundScanMode   mode;
} tSirSmeBackgroundScanModeReq, *tpSirSmeBackgroundScanModeReq;

/// Background Scan Statisics
typedef struct sSirBackgroundScanInfo {
    tANI_U32        numOfScanSuccess;
    tANI_U32        numOfScanFailure;
    tANI_U32        reserved;
} tSirBackgroundScanInfo, *tpSirBackgroundScanInfo;

#define SIR_BACKGROUND_SCAN_INFO_SIZE        (3 * sizeof(tANI_U32))

/// Definition for Authentication request
typedef struct sSirSmeAuthReq
{
    tANI_U16           messageType; // eWNI_SME_AUTH_REQ
    tANI_U16           length;
    tANI_U8            sessionId;        // Session ID
    tANI_U16           transactionId;    // Transaction ID for cmd
    tSirMacAddr        bssId;            // Self BSSID
    tSirMacAddr        peerMacAddr;
    tAniAuthType       authType;
    tANI_U8            channelNumber;
} tSirSmeAuthReq, *tpSirSmeAuthReq;

/// Definition for reponse message to previously issued Auth request
typedef struct sSirSmeAuthRsp
{
    tANI_U16           messageType; // eWNI_SME_AUTH_RSP
    tANI_U16           length;
    tANI_U8            sessionId;      // Session ID
    tANI_U16           transactionId;  // Transaction ID for cmd
    tSirMacAddr        peerMacAddr;
    tAniAuthType       authType;
    tSirResultCodes    statusCode;
    tANI_U16           protStatusCode; //It holds reasonCode when Pre-Auth fails due to deauth frame.
                                       //Otherwise it holds status code.
} tSirSmeAuthRsp, *tpSirSmeAuthRsp;



/// Definition for Join/Reassoc info - Reshmi: need to check if this is a def which moved from elsehwere.
typedef struct sJoinReassocInfo
{
    tAniTitanCBNeighborInfo cbNeighbors;
    tAniBool            spectrumMgtIndicator;
    tSirMacPowerCapInfo powerCap;
    tSirSupChnl         supportedChannels;
} tJoinReassocInfo, *tpJoinReassocInfo;

/// Definition for join request
/// ---> MAC
/// WARNING! If you add a field in JOIN REQ. 
///         Make sure to add it in REASSOC REQ 
/// The Serdes function is the same and its 
/// shared with REASSOC. So if we add a field
//  here and dont add it in REASSOC REQ. It will BREAK!!! REASSOC.
typedef struct sSirSmeJoinReq
{
    tANI_U16            messageType;            // eWNI_SME_JOIN_REQ
    tANI_U16            length;
    tANI_U8             sessionId;
    tANI_U16            transactionId;  
    tSirMacSSid         ssId;
    tSirMacAddr         selfMacAddr;            // self Mac address
    tSirBssType         bsstype;                // add new type for BT -AMP STA and AP Modules
    tANI_U8             dot11mode;              // to support BT-AMP     
    tVOS_CON_MODE       staPersona;             //Persona
    ePhyChanBondState   cbMode;                 // Pass CB mode value in Join.

    /*This contains the UAPSD Flag for all 4 AC
     * B0: AC_VO UAPSD FLAG
     * B1: AC_VI UAPSD FLAG
     * B2: AC_BK UAPSD FLAG
     * B3: AC_BE UASPD FLAG
     */
    tANI_U8                 uapsdPerAcBitmask;

    tSirMacRateSet      operationalRateSet;// Has 11a or 11b rates
    tSirMacRateSet      extendedRateSet;    // Has 11g rates
    tSirRSNie           rsnIE;                  // RSN IE to be sent in
                                                // (Re) Association Request
#ifdef FEATURE_WLAN_ESE
    tSirCCKMie          cckmIE;             // CCMK IE to be included as handler for join and reassoc is 
                                            // the same. The join will never carry cckm, but will be set to
                                            // 0. 
#endif

    tSirAddie           addIEScan;              // Additional IE to be sent in
                                                // (unicast) Probe Request at the time of join

    tSirAddie           addIEAssoc;             // Additional IE to be sent in 
                                                // (Re) Association Request

    tAniEdType          UCEncryptionType;

    tAniEdType          MCEncryptionType;

#ifdef WLAN_FEATURE_11W
    tAniEdType          MgmtEncryptionType;
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
    tAniBool            is11Rconnection;
#endif
#ifdef FEATURE_WLAN_ESE
    tAniBool            isESEFeatureIniEnabled;
    tAniBool            isESEconnection;
    tESETspecInfo       eseTspecInfo;
#endif
    
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
    tAniBool            isFastTransitionEnabled;
#endif
#ifdef FEATURE_WLAN_LFR
    tAniBool            isFastRoamIniFeatureEnabled;
#endif

    tANI_U8             txLdpcIniFeatureEnabled;
#ifdef WLAN_FEATURE_11AC
    tANI_U8             txBFIniFeatureEnabled;
    tANI_U8             txBFCsnValue;
    tANI_U8             txMuBformee;
#endif
    tANI_U8             isAmsduSupportInAMPDU;
    tAniBool            isWMEenabled;
    tAniBool            isQosEnabled;
    tAniTitanCBNeighborInfo cbNeighbors;
    tAniBool            spectrumMgtIndicator;
    tSirMacPowerCapInfo powerCap;
    tSirSupChnl         supportedChannels;
    tSirBssDescription  bssDescription;

} tSirSmeJoinReq, *tpSirSmeJoinReq;

/// Definition for reponse message to previously issued join request
/// MAC --->
typedef struct sSirSmeJoinRsp
{
    tANI_U16                messageType; // eWNI_SME_JOIN_RSP
    tANI_U16                length;
    tANI_U8                 sessionId;         // Session ID
    tANI_U16                transactionId;     // Transaction ID for cmd
    tSirResultCodes    statusCode;
    tAniAuthType       authType;
    tANI_U16        protStatusCode; //It holds reasonCode when join fails due to deauth/disassoc frame.
                                    //Otherwise it holds status code.
    tANI_U16        aid;
    tANI_U32        beaconLength;
    tANI_U32        assocReqLength;
    tANI_U32        assocRspLength;
#ifdef WLAN_FEATURE_VOWIFI_11R
    tANI_U32        parsedRicRspLen;
#endif
#ifdef FEATURE_WLAN_ESE
    tANI_U32        tspecIeLen;
#endif
    tANI_U32        staId;//Station ID for peer

    /*The DPU signatures will be sent eventually to TL to help it determine the 
      association to which a packet belongs to*/
    /*Unicast DPU signature*/
    tANI_U8            ucastSig;

    /*Broadcast DPU signature*/
    tANI_U8            bcastSig;

    /*to report MAX link-speed populate rate-flags from ASSOC RSP frame*/
    tANI_U32           maxRateFlags;

    tANI_U8         frames[ 1 ];
} tSirSmeJoinRsp, *tpSirSmeJoinRsp;

/// Definition for Authentication indication from peer
typedef struct sSirSmeAuthInd
{
    tANI_U16           messageType; // eWNI_SME_AUTH_IND
    tANI_U16           length;         
    tANI_U8            sessionId;
    tSirMacAddr        bssId;             // Self BSSID
    tSirMacAddr        peerMacAddr;
    tAniAuthType       authType;
} tSirSmeAuthInd, *tpSirSmeAuthInd;

/// probereq from peer, when wsc is enabled
typedef struct sSirSmeProbereq
{
    tANI_U16           messageType; // eWNI_SME_PROBE_REQ
    tANI_U16           length;
    tANI_U8            sessionId;
    tSirMacAddr        peerMacAddr;
    tANI_U16           devicePasswdId;
} tSirSmeProbeReq, *tpSirSmeProbeReq;

/// Definition for Association indication from peer
/// MAC --->
typedef struct sSirSmeAssocInd
{
    tANI_U16             messageType; // eWNI_SME_ASSOC_IND
    tANI_U16             length;
    tANI_U8              sessionId;
    tSirMacAddr          peerMacAddr;
    tANI_U16             aid;
    tSirMacAddr          bssId; // Self BSSID
    tANI_U16             staId; // Station ID for peer
    tANI_U8              uniSig;  // DPU signature for unicast packets
    tANI_U8              bcastSig; // DPU signature for broadcast packets
    tAniAuthType         authType;    
    tAniSSID             ssId; // SSID used by STA to associate
    tSirRSNie            rsnIE;// RSN IE received from peer
    tSirAddie            addIE;// Additional IE received from peer, which possibly include WSC IE and/or P2P IE

    // powerCap & supportedChannels are present only when
    // spectrumMgtIndicator flag is set
    tAniBool                spectrumMgtIndicator;
    tSirMacPowerCapInfo     powerCap;
    tSirSupChnl             supportedChannels;
    tAniBool             wmmEnabledSta; /* if present - STA is WMM enabled */
    tAniBool             reassocReq;
    // Required for indicating the frames to upper layer
    tANI_U32             beaconLength;
    tANI_U8*             beaconPtr;
    tANI_U32             assocReqLength;
    tANI_U8*             assocReqPtr;
} tSirSmeAssocInd, *tpSirSmeAssocInd;


/// Definition for Association confirm
/// ---> MAC
typedef struct sSirSmeAssocCnf
{
    tANI_U16             messageType; // eWNI_SME_ASSOC_CNF
    tANI_U16             length;
    tSirResultCodes      statusCode;
    tSirMacAddr          bssId;      // Self BSSID
    tSirMacAddr          peerMacAddr;
    tANI_U16             aid;
    tSirMacAddr          alternateBssId;
    tANI_U8              alternateChannelId;
} tSirSmeAssocCnf, *tpSirSmeAssocCnf;

/// Definition for Reassociation indication from peer
typedef struct sSirSmeReassocInd
{
    tANI_U16            messageType; // eWNI_SME_REASSOC_IND
    tANI_U16            length;
    tANI_U8             sessionId;         // Session ID
    tSirMacAddr         peerMacAddr;
    tSirMacAddr         oldMacAddr;
    tANI_U16            aid;
    tSirMacAddr         bssId;           // Self BSSID
    tANI_U16            staId;           // Station ID for peer
    tAniAuthType        authType;
    tAniSSID            ssId;   // SSID used by STA to reassociate
    tSirRSNie           rsnIE;  // RSN IE received from peer

    tSirAddie           addIE;  // Additional IE received from peer
    
    // powerCap & supportedChannels are present only when
    // spectrumMgtIndicator flag is set
    tAniBool                spectrumMgtIndicator;
    tSirMacPowerCapInfo     powerCap;
    tSirSupChnl             supportedChannels;
    // Required for indicating the frames to upper layer
    // TODO: use the appropriate names to distinguish between the other similar names used above for station mode of operation
    tANI_U32             beaconLength;
    tANI_U8*             beaconPtr;
    tANI_U32             assocReqLength;
    tANI_U8*             assocReqPtr;
} tSirSmeReassocInd, *tpSirSmeReassocInd;

/// Definition for Reassociation confirm
/// ---> MAC
typedef struct sSirSmeReassocCnf
{
    tANI_U16                  messageType; // eWNI_SME_REASSOC_CNF
    tANI_U16                  length;
    tSirResultCodes      statusCode;
    tSirMacAddr          bssId;             // Self BSSID
    tSirMacAddr          peerMacAddr;
    tANI_U16                  aid;
    tSirMacAddr          alternateBssId;
    tANI_U8                   alternateChannelId;
} tSirSmeReassocCnf, *tpSirSmeReassocCnf;


/// Enum definition for  Wireless medium status change codes
typedef enum eSirSmeStatusChangeCode
{
    eSIR_SME_DEAUTH_FROM_PEER,
    eSIR_SME_DISASSOC_FROM_PEER,
    eSIR_SME_LOST_LINK_WITH_PEER,
    eSIR_SME_CHANNEL_SWITCH,
    eSIR_SME_JOINED_NEW_BSS,
    eSIR_SME_LEAVING_BSS,
    eSIR_SME_IBSS_ACTIVE,
    eSIR_SME_IBSS_INACTIVE,
    eSIR_SME_IBSS_PEER_DEPARTED,
    eSIR_SME_RADAR_DETECTED,
    eSIR_SME_IBSS_NEW_PEER,
    eSIR_SME_AP_CAPS_CHANGED,
    eSIR_SME_BACKGROUND_SCAN_FAIL,
    eSIR_SME_CB_LEGACY_BSS_FOUND_BY_AP,
    eSIR_SME_CB_LEGACY_BSS_FOUND_BY_STA
} tSirSmeStatusChangeCode;

typedef struct sSirSmeNewBssInfo
{
    tSirMacAddr   bssId;
    tANI_U8            channelNumber;
    tANI_U8            reserved;
    tSirMacSSid   ssId;
} tSirSmeNewBssInfo, *tpSirSmeNewBssInfo;

typedef struct sSirSmeApNewCaps
{
    tANI_U16           capabilityInfo;
    tSirMacAddr   bssId;
    tANI_U8            channelId;
    tANI_U8            reserved[3];
    tSirMacSSid   ssId;
} tSirSmeApNewCaps, *tpSirSmeApNewCaps;

/**
 * Table below indicates what information is passed for each of
 * the Wireless Media status change notifications:
 *
 * Status Change code           Status change info
 * ----------------------------------------------------------------------
 * eSIR_SME_DEAUTH_FROM_PEER        Reason code received in DEAUTH frame
 * eSIR_SME_DISASSOC_FROM_PEER      Reason code received in DISASSOC frame
 * eSIR_SME_LOST_LINK_WITH_PEER     None
 * eSIR_SME_CHANNEL_SWITCH          New channel number
 * eSIR_SME_JOINED_NEW_BSS          BSSID, SSID and channel number
 * eSIR_SME_LEAVING_BSS             None
 * eSIR_SME_IBSS_ACTIVE             Indicates that another STA joined
 *                                  IBSS apart from this STA that
 *                                  started IBSS
 * eSIR_SME_IBSS_INACTIVE           Indicates that only this STA is left
 *                                  in IBSS
 * eSIR_SME_RADAR_DETECTED          Indicates that radar is detected
 * eSIR_SME_IBSS_NEW_PEER           Indicates that a new peer is detected
 * eSIR_SME_AP_CAPS_CHANGED         Indicates that capabilities of the AP
 *                                  that STA is currently associated with
 *                                  have changed.
 * eSIR_SME_BACKGROUND_SCAN_FAIL    Indicates background scan failure
 */

/// Definition for Wireless medium status change notification
typedef struct sSirSmeWmStatusChangeNtf
{
    tANI_U16                     messageType; // eWNI_SME_WM_STATUS_CHANGE_NTF
    tANI_U16                     length;
    tANI_U8                      sessionId;         // Session ID
    tSirSmeStatusChangeCode statusChangeCode;
    tSirMacAddr             bssId;             // Self BSSID
    union
    {
        tANI_U16                 deAuthReasonCode; // eSIR_SME_DEAUTH_FROM_PEER
        tANI_U16                 disassocReasonCode; // eSIR_SME_DISASSOC_FROM_PEER
        // none for eSIR_SME_LOST_LINK_WITH_PEER
        tANI_U8                  newChannelId;   // eSIR_SME_CHANNEL_SWITCH
        tSirSmeNewBssInfo   newBssInfo;     // eSIR_SME_JOINED_NEW_BSS
        // none for eSIR_SME_LEAVING_BSS
        // none for eSIR_SME_IBSS_ACTIVE
        // none for eSIR_SME_IBSS_INACTIVE
        tSirNewIbssPeerInfo     newIbssPeerInfo;  // eSIR_SME_IBSS_NEW_PEER
        tSirSmeApNewCaps        apNewCaps;        // eSIR_SME_AP_CAPS_CHANGED
        tSirBackgroundScanInfo  bkgndScanInfo;    // eSIR_SME_BACKGROUND_SCAN_FAIL
        tAniTitanCBNeighborInfo cbNeighbors;      // eSIR_SME_CB_LEGACY_BSS_FOUND_BY_STA
    } statusChangeInfo;
} tSirSmeWmStatusChangeNtf, *tpSirSmeWmStatusChangeNtf;

/// Definition for Disassociation request
typedef
__ani_attr_pre_packed
struct sSirSmeDisassocReq
{
    tANI_U16            messageType; // eWNI_SME_DISASSOC_REQ
    tANI_U16            length;
    tANI_U8             sessionId;         // Session ID
    tANI_U16            transactionId;     // Transaction ID for cmd
    tSirMacAddr         bssId;             // Peer BSSID
    tSirMacAddr peerMacAddr;
    tANI_U16         reasonCode;
    tANI_U8          doNotSendOverTheAir;  //This flag tells LIM whether to send the disassoc OTA or not
                                           //This will be set in while handing off from one AP to other
}
__ani_attr_packed
tSirSmeDisassocReq, *tpSirSmeDisassocReq;

/// Definition for Tkip countermeasures request
typedef __ani_attr_pre_packed struct sSirSmeTkipCntrMeasReq
{
    tANI_U16            messageType;    // eWNI_SME_DISASSOC_REQ
    tANI_U16            length;
    tANI_U8             sessionId;      // Session ID
    tANI_U16            transactionId;  // Transaction ID for cmd
    tSirMacAddr         bssId;          // Peer BSSID
    tANI_BOOLEAN        bEnable;        // Start/stop countermeasures
} __ani_attr_packed tSirSmeTkipCntrMeasReq, *tpSirSmeTkipCntrMeasReq;

typedef struct sAni64BitCounters
{
    tANI_U32 Hi;
    tANI_U32 Lo;
}tAni64BitCounters, *tpAni64BitCounters;

typedef struct sAniSecurityStat
{
    tAni64BitCounters txBlks;
    tAni64BitCounters rxBlks;
    tAni64BitCounters formatErrorCnt;
    tAni64BitCounters decryptErr;
    tAni64BitCounters protExclCnt;
    tAni64BitCounters unDecryptableCnt;
    tAni64BitCounters decryptOkCnt;

}tAniSecurityStat, *tpAniSecurityStat;

typedef struct sAniTxRxCounters
{
    tANI_U32 txFrames; // Incremented for every packet tx
    tANI_U32 rxFrames;    
    tANI_U32 nRcvBytes;
    tANI_U32 nXmitBytes;
}tAniTxRxCounters, *tpAniTxRxCounters;

typedef struct sAniTxRxStats
{
    tAni64BitCounters txFrames;
    tAni64BitCounters rxFrames;
    tAni64BitCounters nRcvBytes;
    tAni64BitCounters nXmitBytes;

}tAniTxRxStats,*tpAniTxRxStats;

typedef struct sAniSecStats
{
    tAniSecurityStat aes;
    tAni64BitCounters aesReplays;
    tAniSecurityStat tkip;
    tAni64BitCounters tkipReplays;
    tAni64BitCounters tkipMicError;

    tAniSecurityStat wep;
#if defined(FEATURE_WLAN_WAPI) && !defined(LIBRA_WAPI_SUPPORT)
    tAniSecurityStat wpi;
    tAni64BitCounters wpiReplays;
    tAni64BitCounters wpiMicError;
#endif
}tAniSecStats, *tpAniSecStats;    

#define SIR_MAX_RX_CHAINS 3

typedef struct sAniStaStatStruct
{
    /* following statistic elements till expandPktRxCntLo are not filled with valid data.
     * These are kept as it is, since WSM is using this structure.
     * These elements can be removed whenever WSM is updated.
     * Phystats is used to hold phystats from BD.
     */
    tANI_U32 sentAesBlksUcastHi;
    tANI_U32 sentAesBlksUcastLo;
    tANI_U32 recvAesBlksUcastHi;
    tANI_U32 recvAesBlksUcastLo;
    tANI_U32 aesFormatErrorUcastCnts;
    tANI_U32 aesReplaysUcast;
    tANI_U32 aesDecryptErrUcast;
    tANI_U32 singleRetryPkts;
    tANI_U32 failedTxPkts;
    tANI_U32 ackTimeouts;
    tANI_U32 multiRetryPkts;
    tANI_U32 fragTxCntsHi;
    tANI_U32 fragTxCntsLo;
    tANI_U32 transmittedPktsHi;
    tANI_U32 transmittedPktsLo;
    tANI_U32 phyStatHi; //These are used to fill in the phystats.
    tANI_U32 phyStatLo; //This is only for private use.

    tANI_U32 uplinkRssi;
    tANI_U32 uplinkSinr;
    tANI_U32 uplinkRate;
    tANI_U32 downlinkRssi;
    tANI_U32 downlinkSinr;
    tANI_U32 downlinkRate;
    tANI_U32 nRcvBytes;
    tANI_U32 nXmitBytes;

    // titan 3c stats
    tANI_U32 chunksTxCntHi;          // Number of Chunks Transmitted
    tANI_U32 chunksTxCntLo;
    tANI_U32 compPktRxCntHi;         // Number of Packets Received that were actually compressed
    tANI_U32 compPktRxCntLo;
    tANI_U32 expanPktRxCntHi;        // Number of Packets Received that got expanded
    tANI_U32 expanPktRxCntLo;


    /* Following elements are valid and filled in correctly. They have valid values.
     */

    //Unicast frames and bytes.
    tAniTxRxStats ucStats;

    //Broadcast frames and bytes.
    tAniTxRxStats bcStats;

    //Multicast frames and bytes.
    tAniTxRxStats mcStats;

    tANI_U32      currentTxRate; 
    tANI_U32      currentRxRate; //Rate in 100Kbps

    tANI_U32      maxTxRate;
    tANI_U32      maxRxRate;

    tANI_S8       rssi[SIR_MAX_RX_CHAINS]; 


    tAniSecStats   securityStats;

    tANI_U8       currentRxRateIdx; //This the softmac rate Index.
    tANI_U8       currentTxRateIdx;

} tAniStaStatStruct, *tpAniStaStatStruct;

//Statistics that are not maintained per stations.
typedef struct sAniGlobalStatStruct
{
  tAni64BitCounters txError;
  tAni64BitCounters rxError;
  tAni64BitCounters rxDropNoBuffer;
  tAni64BitCounters rxDropDup;
  tAni64BitCounters rxCRCError;

  tAni64BitCounters singleRetryPkts;
  tAni64BitCounters failedTxPkts;
  tAni64BitCounters ackTimeouts;
  tAni64BitCounters multiRetryPkts;
  tAni64BitCounters fragTxCnts;
  tAni64BitCounters fragRxCnts;

  tAni64BitCounters txRTSSuccess;
  tAni64BitCounters txCTSSuccess;
  tAni64BitCounters rxRTSSuccess;
  tAni64BitCounters rxCTSSuccess;

  tAniSecStats      securityStats;

  tAniTxRxStats     mcStats;
  tAniTxRxStats     bcStats;
    
}tAniGlobalStatStruct,*tpAniGlobalStatStruct;

typedef enum sPacketType
{
    ePACKET_TYPE_UNKNOWN,
    ePACKET_TYPE_11A,
    ePACKET_TYPE_11G,
    ePACKET_TYPE_11B,
    ePACKET_TYPE_11N

}tPacketType, *tpPacketType;

typedef struct sAniStatSummaryStruct
{
    tAniTxRxStats uc; //Unicast counters.
    tAniTxRxStats bc; //Broadcast counters.
    tAniTxRxStats mc; //Multicast counters.
    tAni64BitCounters txError;
    tAni64BitCounters rxError;
    tANI_S8     rssi[SIR_MAX_RX_CHAINS]; //For each chain.
    tANI_U32    rxRate; // Rx rate of the last received packet.
    tANI_U32    txRate;
    tANI_U16    rxMCSId; //MCS index is valid only when packet type is ePACKET_TYPE_11N
    tANI_U16    txMCSId;
    tPacketType rxPacketType;
    tPacketType txPacketType;
    tSirMacAddr macAddr; //Mac Address of the station from which above RSSI and rate is from.
}tAniStatSummaryStruct,*tpAniStatSummaryStruct;

//structure for stats that may be reset, like the ones in sta descriptor
//The stats are saved into here before reset. It should be tANI_U32 aligned.
typedef struct _sPermStaStats
{
    //tANI_U32 sentAesBlksUcastHi;
    //tANI_U32 sentAesBlksUcastLo;
    //tANI_U32 recvAesBlksUcastHi;
    //tANI_U32 recvAesBlksUcastLo;
    tANI_U32 aesFormatErrorUcastCnts;
    tANI_U32 aesReplaysUcast;
    tANI_U32 aesDecryptErrUcast;
    tANI_U32 singleRetryPkts;
    tANI_U32 failedTxPkts;
    tANI_U32 ackTimeouts;
    tANI_U32 multiRetryPkts;
    tANI_U32 fragTxCntsHi;
    tANI_U32 fragTxCntsLo;
    tANI_U32 transmittedPktsHi;
    tANI_U32 transmittedPktsLo;

    // titan 3c stats
    tANI_U32 chunksTxCntHi;          // Number of Chunks Transmitted
    tANI_U32 chunksTxCntLo;
    tANI_U32 compPktRxCntHi;         // Number of Packets Received that were actually compressed
    tANI_U32 compPktRxCntLo;
    tANI_U32 expanPktRxCntHi;        // Number of Packets Received that got expanded
    tANI_U32 expanPktRxCntLo;
}tPermanentStaStats;




/// Definition for Disassociation response
typedef struct sSirSmeDisassocRsp
{
    tANI_U16           messageType; // eWNI_SME_DISASSOC_RSP
    tANI_U16           length;
    tANI_U8            sessionId;         // Session ID
    tANI_U16           transactionId;     // Transaction ID for cmd
    tSirResultCodes    statusCode;
    tSirMacAddr        peerMacAddr;
    tAniStaStatStruct  perStaStats; // STA stats
    tANI_U16           staId;
}
__ani_attr_packed
 tSirSmeDisassocRsp, *tpSirSmeDisassocRsp;

/// Definition for Disassociation indication from peer
typedef struct sSirSmeDisassocInd
{
    tANI_U16            messageType; // eWNI_SME_DISASSOC_IND
    tANI_U16            length;
    tANI_U8             sessionId;  // Session Identifier
    tANI_U16            transactionId;   // Transaction Identifier with PE
    tSirResultCodes     statusCode;
    tSirMacAddr         bssId;            
    tSirMacAddr         peerMacAddr;
    tAniStaStatStruct  perStaStats; // STA stats
    tANI_U16            staId;
    tANI_U32            reasonCode;
} tSirSmeDisassocInd, *tpSirSmeDisassocInd;

/// Definition for Disassociation confirm
/// MAC --->
typedef struct sSirSmeDisassocCnf
{
    tANI_U16            messageType; // eWNI_SME_DISASSOC_CNF
    tANI_U16            length;
    tSirResultCodes     statusCode;
    tSirMacAddr         bssId;            
    tSirMacAddr         peerMacAddr;
} tSirSmeDisassocCnf, *tpSirSmeDisassocCnf;

/// Definition for Deauthetication request
typedef struct sSirSmeDeauthReq
{
    tANI_U16            messageType;   // eWNI_SME_DEAUTH_REQ
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tSirMacAddr         bssId;         // AP BSSID
    tSirMacAddr         peerMacAddr;
    tANI_U16            reasonCode;
} tSirSmeDeauthReq, *tpSirSmeDeauthReq;

/// Definition for Deauthetication response
typedef struct sSirSmeDeauthRsp
{
    tANI_U16                messageType; // eWNI_SME_DEAUTH_RSP
    tANI_U16                length;
    tANI_U8             sessionId;         // Session ID
    tANI_U16            transactionId;     // Transaction ID for cmd
    tSirResultCodes     statusCode;
    tSirMacAddr        peerMacAddr;
} tSirSmeDeauthRsp, *tpSirSmeDeauthRsp;

/// Definition for Deauthetication indication from peer
typedef struct sSirSmeDeauthInd
{
    tANI_U16            messageType; // eWNI_SME_DEAUTH_IND
    tANI_U16            length;
    tANI_U8            sessionId;       //Added for BT-AMP
    tANI_U16            transactionId;  //Added for BT-AMP
    tSirResultCodes     statusCode;
    tSirMacAddr         bssId;// AP BSSID
    tSirMacAddr         peerMacAddr;

    tANI_U16            staId;
    tANI_U32            reasonCode;
} tSirSmeDeauthInd, *tpSirSmeDeauthInd;

/// Definition for Deauthentication confirm
/// MAC --->
typedef struct sSirSmeDeauthCnf
{
    tANI_U16                messageType; // eWNI_SME_DEAUTH_CNF
    tANI_U16                length;
    tSirResultCodes     statusCode;
    tSirMacAddr         bssId;             // AP BSSID
    tSirMacAddr        peerMacAddr;
} tSirSmeDeauthCnf, *tpSirSmeDeauthCnf;

/// Definition for stop BSS request message
typedef struct sSirSmeStopBssReq
{
    tANI_U16                messageType;    // eWNI_SME_STOP_BSS_REQ
    tANI_U16                length;
    tANI_U8             sessionId;      //Session ID
    tANI_U16            transactionId;  //tranSaction ID for cmd
    tSirResultCodes         reasonCode;
    tSirMacAddr             bssId;          //Self BSSID
} tSirSmeStopBssReq, *tpSirSmeStopBssReq;

/// Definition for stop BSS response message
typedef struct sSirSmeStopBssRsp
{
    tANI_U16             messageType; // eWNI_SME_STOP_BSS_RSP
    tANI_U16             length;
    tSirResultCodes statusCode;
    tANI_U8             sessionId;         // Session ID
    tANI_U16            transactionId;     // Transaction ID for cmd
} tSirSmeStopBssRsp, *tpSirSmeStopBssRsp;



/// Definition for Channel Switch indication for station
/// MAC --->
typedef struct sSirSmeSwitchChannelInd
{
    tANI_U16                messageType; // eWNI_SME_SWITCH_CHL_REQ
    tANI_U16                length;
    tANI_U8                 sessionId;
    tANI_U16    newChannelId;
    tSirMacAddr        bssId;      // BSSID
} tSirSmeSwitchChannelInd, *tpSirSmeSwitchChannelInd;

/// Definition for ULA complete indication message
typedef struct sirUlaCompleteInd
{
    tANI_U16                messageType; // eWNI_ULA_COMPLETE_IND
    tANI_U16                length;
    tSirResultCodes    statusCode;
    tSirMacAddr        peerMacAddr;
} tSirUlaCompleteInd, *tpSirUlaCompleteInd;

/// Definition for ULA complete confirmation message
typedef struct sirUlaCompleteCnf
{
    tANI_U16                messageType; // eWNI_ULA_COMPLETE_CNF
    tANI_U16                length;
    tSirResultCodes    statusCode;
    tSirMacAddr        peerMacAddr;
} tSirUlaCompleteCnf, *tpSirUlaCompleteCnf;

/// Definition for Neighbor BSS indication
/// MAC --->
/// MAC reports this each time a new I/BSS is detected
typedef struct sSirSmeNeighborBssInd
{
    tANI_U16                    messageType; // eWNI_SME_NEIGHBOR_BSS_IND
    tANI_U16                    length;
    tANI_U8                     sessionId;
    tSirBssDescription     bssDescription[1];
} tSirSmeNeighborBssInd, *tpSirSmeNeighborBssInd;

/// Definition for MIC failure indication
/// MAC --->
/// MAC reports this each time a MIC failure occures on Rx TKIP packet
typedef struct sSirSmeMicFailureInd
{
    tANI_U16                    messageType; // eWNI_SME_MIC_FAILURE_IND
    tANI_U16                    length;
    tANI_U8                     sessionId;
    tSirMacAddr         bssId;             // BSSID
    tSirMicFailureInfo     info;
} tSirSmeMicFailureInd, *tpSirSmeMicFailureInd;

typedef struct sSirSmeMissedBeaconInd
{
    tANI_U16                    messageType; // eWNI_SME_MISSED_BEACON_IND
    tANI_U16                    length;
    tANI_U8                     bssIdx;
} tSirSmeMissedBeaconInd, *tpSirSmeMissedBeaconInd;

/// Definition for Set Context request
/// ---> MAC
typedef struct sSirSmeSetContextReq
{
    tANI_U16           messageType; // eWNI_SME_SET_CONTEXT_REQ
    tANI_U16          length;
    tANI_U8            sessionId;  //Session ID
    tANI_U16           transactionId; //Transaction ID for cmd
    tSirMacAddr        peerMacAddr;
    tSirMacAddr        bssId;      // BSSID
    // TBD Following QOS fields to be uncommented
    //tAniBool           qosInfoPresent;
    //tSirQos            qos;
    tSirKeyMaterial    keyMaterial;
} tSirSmeSetContextReq, *tpSirSmeSetContextReq;

/// Definition for Set Context response
/// MAC --->
typedef struct sSirSmeSetContextRsp
{
    tANI_U16                messageType; // eWNI_SME_SET_CONTEXT_RSP
    tANI_U16                length;
    tANI_U8             sessionId;         // Session ID
    tANI_U16            transactionId;     // Transaction ID for cmd
    tSirResultCodes     statusCode;
    tSirMacAddr             peerMacAddr;
} tSirSmeSetContextRsp, *tpSirSmeSetContextRsp;

/// Definition for Remove Key Context request
/// ---> MAC
typedef struct sSirSmeRemoveKeyReq
{
    tANI_U16                messageType;    // eWNI_SME_REMOVE_KEY_REQ
    tANI_U16                length;
    tANI_U8             sessionId;         // Session ID
    tANI_U16            transactionId;     // Transaction ID for cmd
    tSirMacAddr         bssId;             // BSSID
    tSirMacAddr             peerMacAddr;
    tANI_U8    edType;
    tANI_U8    wepType;
    tANI_U8    keyId;
    tANI_BOOLEAN unicast;
} tSirSmeRemoveKeyReq, *tpSirSmeRemoveKeyReq;

/// Definition for Remove Key Context response
/// MAC --->
typedef struct sSirSmeRemoveKeyRsp
{
    tANI_U16                messageType; // eWNI_SME_REMOVE_KEY_RSP
    tANI_U16                length;
    tANI_U8             sessionId;         // Session ID
    tANI_U16            transactionId;     // Transaction ID for cmd
    tSirResultCodes     statusCode;
    tSirMacAddr             peerMacAddr;
} tSirSmeRemoveKeyRsp, *tpSirSmeRemoveKeyRsp;

/// Definition for Set Power request
/// ---> MAC
typedef struct sSirSmeSetPowerReq
{
    tANI_U16                messageType; // eWNI_SME_SET_POWER_REQ
    tANI_U16                length;
    tANI_U16            transactionId;     // Transaction ID for cmd
    tANI_S8                 powerLevel;
} tSirSmeSetPowerReq, *tpSirSmeSetPowerReq;

/// Definition for Set Power response
/// MAC --->
typedef struct sSirSmeSetPowerRsp
{
    tANI_U16                messageType; // eWNI_SME_SET_POWER_RSP
    tANI_U16                length;
    tSirResultCodes    statusCode;
    tANI_U16            transactionId;     // Transaction ID for cmd
} tSirSmeSetPowerRsp, *tpSirSmeSetPowerRsp;


/// Definition for Link Test Start response
/// MAC --->
typedef struct sSirSmeLinkTestStartRsp
{
    tANI_U16                messageType; // eWNI_SME_LINK_TEST_START_RSP
    tANI_U16                length;
    tSirMacAddr        peerMacAddr;
    tSirResultCodes    statusCode;
} tSirSmeLinkTestStartRsp, *tpSirSmeLinkTestStartRsp;

/// Definition for Link Test Stop response
/// WSM ---> MAC
typedef struct sSirSmeLinkTestStopRsp
{
    tANI_U16                messageType; // eWNI_SME_LINK_TEST_STOP_RSP
    tANI_U16                length;
    tSirMacAddr        peerMacAddr;
    tSirResultCodes    statusCode;
} tSirSmeLinkTestStopRsp, *tpSirSmeLinkTestStopRsp;

/// Definition for kick starting DFS measurements
typedef struct sSirSmeDFSreq
{
    tANI_U16             messageType; // eWNI_SME_DFS_REQ
    tANI_U16             length;
    tANI_U16            transactionId;     // Transaction ID for cmd
} tSirSmeDFSrequest, *tpSirSmeDFSrequest;

/// Definition for response message to previously
/// issued DFS request
typedef struct sSirSmeDFSrsp
{
    tANI_U16             messageType; // eWNI_SME_DFS_RSP
    tANI_U16             length;
    tSirResultCodes statusCode;
    tANI_U16            transactionId;     // Transaction ID for cmd
    tANI_U32             dfsReport[1];
} tSirSmeDFSrsp, *tpSirSmeDFSrsp;

/// Statistic definitions
//=============================================================
// Per STA statistic structure; This same struct will be used for Aggregate
// STA stats as well.

// Clear radio stats and clear per sta stats
typedef enum
{
    eANI_CLEAR_ALL_STATS, // Clears all stats
    eANI_CLEAR_RX_STATS,  // Clears RX statistics of the radio interface
    eANI_CLEAR_TX_STATS,  // Clears TX statistics of the radio interface
    eANI_CLEAR_RADIO_STATS,   // Clears all the radio stats
    eANI_CLEAR_PER_STA_STATS, // Clears Per STA stats
    eANI_CLEAR_AGGR_PER_STA_STATS, // Clears aggregate stats

    // Used to distinguish between per sta to security stats.
    // Used only by AP, FW just returns the same parameter as it received.
    eANI_LINK_STATS,     // Get Per STA stats
    eANI_SECURITY_STATS, // Get Per STA security stats

    eANI_CLEAR_STAT_TYPES_END
} tAniStatSubTypes;

typedef struct sAniTxCtrs
{
    // add the rate counters here
    tANI_U32 tx1Mbps;
    tANI_U32 tx2Mbps;
    tANI_U32 tx5_5Mbps;
    tANI_U32 tx6Mbps;
    tANI_U32 tx9Mbps;
    tANI_U32 tx11Mbps;
    tANI_U32 tx12Mbps;
    tANI_U32 tx18Mbps;
    tANI_U32 tx24Mbps;
    tANI_U32 tx36Mbps;
    tANI_U32 tx48Mbps;
    tANI_U32 tx54Mbps;
    tANI_U32 tx72Mbps;
    tANI_U32 tx96Mbps;
    tANI_U32 tx108Mbps;

    // tx path radio counts
    tANI_U32 txFragHi;
    tANI_U32 txFragLo;
    tANI_U32 txFrameHi;
    tANI_U32 txFrameLo;
    tANI_U32 txMulticastFrameHi;
    tANI_U32 txMulticastFrameLo;
    tANI_U32 txFailedHi;
    tANI_U32 txFailedLo;
    tANI_U32 multipleRetryHi;
    tANI_U32 multipleRetryLo;
    tANI_U32 singleRetryHi;
    tANI_U32 singleRetryLo;
    tANI_U32 ackFailureHi;
    tANI_U32 ackFailureLo;
    tANI_U32 xmitBeacons;

    // titan 3c stats
    tANI_U32 txCbEscPktCntHi;            // Total Number of Channel Bonded/Escort Packet Transmitted
    tANI_U32 txCbEscPktCntLo;
    tANI_U32 txChunksCntHi;              // Total Number of Chunks Transmitted
    tANI_U32 txChunksCntLo;
    tANI_U32 txCompPktCntHi;             // Total Number of Compresssed Packet Transmitted
    tANI_U32 txCompPktCntLo;
    tANI_U32 tx50PerCompPktCntHi;        // Total Number of Packets with 50% or more compression
    tANI_U32 tx50PerCompPktCntLo;
    tANI_U32 txExpanPktCntHi;            // Total Number of Packets Transmitted that got expanded
    tANI_U32 txExpanPktCntLo;
} tAniTxCtrs, *tpAniTxCtrs;

typedef struct sAniRxCtrs
{
    // receive frame rate counters
    tANI_U32 rx1Mbps;
    tANI_U32 rx2Mbps;
    tANI_U32 rx5_5Mbps;
    tANI_U32 rx6Mbps;
    tANI_U32 rx9Mbps;
    tANI_U32 rx11Mbps;
    tANI_U32 rx12Mbps;
    tANI_U32 rx18Mbps;
    tANI_U32 rx24Mbps;
    tANI_U32 rx36Mbps;
    tANI_U32 rx48Mbps;
    tANI_U32 rx54Mbps;
    tANI_U32 rx72Mbps;
    tANI_U32 rx96Mbps;
    tANI_U32 rx108Mbps;

    // receive size counters; 'Lte' = Less than or equal to
    tANI_U32 rxLte64;
    tANI_U32 rxLte128Gt64;
    tANI_U32 rxLte256Gt128;
    tANI_U32 rxLte512Gt256;
    tANI_U32 rxLte1kGt512;
    tANI_U32 rxLte1518Gt1k;
    tANI_U32 rxLte2kGt1518;
    tANI_U32 rxLte4kGt2k;

    // rx radio stats
    tANI_U32 rxFrag;
    tANI_U32 rxFrame;
    tANI_U32 fcsError;
    tANI_U32 rxMulticast;
    tANI_U32 duplicate;
    tANI_U32 rtsSuccess;
    tANI_U32 rtsFailed;
    tANI_U32 wepUndecryptables;
    tANI_U32 drops;
    tANI_U32 aesFormatErrorUcastCnts;
    tANI_U32 aesReplaysUcast;
    tANI_U32 aesDecryptErrUcast;

    // titan 3c stats
    tANI_U32 rxDecompPktCntHi;           // Total Number of Packets that got decompressed
    tANI_U32 rxDecompPktCntLo;
    tANI_U32 rxCompPktCntHi;             // Total Number of Packets received that were actually compressed
    tANI_U32 rxCompPktCntLo;
    tANI_U32 rxExpanPktCntHi;            // Total Number of Packets received that got expanded
    tANI_U32 rxExpanPktCntLo;
} tAniRxCtrs, *tpAniRxCtrs;

// Radio stats
typedef struct sAniRadioStats
{
    tAniTxCtrs tx;
    tAniRxCtrs rx;
} tAniRadioStats, *tpAniRadioStats;

// Get Radio Stats request structure
// This structure shall be used for both Radio stats and Aggregate stats
// A valid request must contain entire structure with/without valid fields.
// Based on the request type, the valid fields will be checked.
typedef struct sAniGetStatsReq
{
    // Common for all types are requests
    tANI_U16                msgType;    // message type is same as the request type
    tANI_U16                msgLen;     // length of the entire request
    tANI_U8                 sessionId;  //Session ID
    tANI_U16                transactionId;
    tSirMacAddr             bssId;      //BSSID
    // only used for clear stats and per sta stats clear
    tAniStatSubTypes        stat;   // Clears the stats of the described types.
    tANI_U32                staId;  // Per STA stats request must contain valid
                               // values
    tANI_U8                 macAddr[6];
} tAniGetStatsReq, *tpAniGetStatsReq;

// Get Radio Stats response struct
typedef struct sAniGetRadioStatsRsp
{
    tANI_U16            type;   // message type is same as the request type
    tANI_U16            msgLen; // length of the entire request
    tANI_U32            rc;
    tANI_U16            transactionId;
    tAniRadioStats radio;
} tAniGetRadioStatsRsp, *tpAniGetRadioStatsRsp;

// Per Sta stats response struct
typedef struct sAniGetPerStaStatsRsp
{
    tANI_U16               type;   // message type is same as the request type
    tANI_U16               msgLen; // length of the entire request
    tANI_U32               rc;
    tANI_U16               transactionId;
    tAniStatSubTypes  stat;   // Sub type needed by AP. Returns the same value
    tAniStaStatStruct sta;
    tANI_U32               staId;
    tANI_U8                macAddr[6];
} tAniGetPerStaStatsRsp, *tpAniGetPerStaStatsRsp;

// Get Aggregate stats
typedef struct sAniGetAggrStaStatsRsp
{
    tANI_U16               type;   // message type is same as the request type
    tANI_U16               msgLen; // length of the entire request
    tANI_U32               rc;
    tANI_U16               transactionId;
    tAniStaStatStruct sta;
} tAniGetAggrStaStatsRsp, *tpAniGetAggrStaStatsRsp;

// Clear stats request and response structure. 'rc' field is unused in
// request and this field is used in response field.
typedef struct sAniClearStatsRsp
{
    tANI_U16                type;   // message type is same as the request type
    tANI_U16                msgLen; // length of the entire request
    tANI_U32                rc;     // return code - will be filled by FW on
                               // response.
                       // Same transaction ID will be returned by the FW
    tANI_U16                transactionId;
    tAniStatSubTypes   stat;       // Clears the stats of the described types.
    tANI_U32                staId;      // Applicable only to PER STA stats clearing
    tANI_U8                 macAddr[6]; // Applicable only to PER STA stats clearing
} tAniClearStatsRsp, *tpAniClearStatsRsp;

typedef struct sAniGetGlobalStatsRsp
{
    tANI_U16            type;   // message type is same as the request type
    tANI_U16            msgLen; // length of the entire request
    tANI_U32            rc;
    tANI_U16            transactionId;
    tAniGlobalStatStruct global;
} tAniGetGlobalStatsRsp, *tpAniGetGlobalStatsRsp;

typedef struct sAniGetStatSummaryRsp
{
    tANI_U16               type;   // message type is same as the request type
    tANI_U16               msgLen; // length of the entire request --Why?
    tANI_U32               rc;
    tANI_U16               transactionId;
    tAniStatSummaryStruct stat;
} tAniGetStatSummaryRsp, *tpAniGetStatSummaryRsp;

//***************************************************************


/*******************PE Statistics*************************/
typedef enum
{
    PE_SUMMARY_STATS_INFO           = 0x00000001,
    PE_GLOBAL_CLASS_A_STATS_INFO    = 0x00000002,
    PE_GLOBAL_CLASS_B_STATS_INFO    = 0x00000004,
    PE_GLOBAL_CLASS_C_STATS_INFO    = 0x00000008,
    PE_GLOBAL_CLASS_D_STATS_INFO    = 0x00000010,
    PE_PER_STA_STATS_INFO           = 0x00000020
}ePEStatsMask;

/*
 * tpAniGetPEStatsReq is tied to 
 * for SME ==> PE eWNI_SME_GET_STATISTICS_REQ msgId  and 
 * for PE ==> HAL SIR_HAL_GET_STATISTICS_REQ msgId
 */
typedef struct sAniGetPEStatsReq
{
    // Common for all types are requests
    tANI_U16                msgType;    // message type is same as the request type
    tANI_U16                msgLen;  // length of the entire request
    tANI_U32                staId;  // Per STA stats request must contain valid
    tANI_U32                statsMask;  // categories of stats requested. look at ePEStatsMask
} tAniGetPEStatsReq, *tpAniGetPEStatsReq;

/*
 * tpAniGetPEStatsRsp is tied to 
 * for PE ==> SME eWNI_SME_GET_STATISTICS_RSP msgId  and 
 * for HAL ==> PE SIR_HAL_GET_STATISTICS_RSP msgId
 */
typedef struct sAniGetPEStatsRsp
{
    // Common for all types are responses
    tANI_U16                msgType;    // message type is same as the request type
    tANI_U16                msgLen;  // length of the entire request, includes the pStatsBuf length too
    tANI_U8                  sessionId;
    tANI_U32                rc;         //success/failure
    tANI_U32                staId;  // Per STA stats request must contain valid
    tANI_U32                statsMask;  // categories of stats requested. look at ePEStatsMask
/**********************************************************************************************
    //void                  *pStatsBuf;
    The Stats buffer starts here and can be an aggregate of more than one statistics 
    structure depending on statsMask.The void pointer "pStatsBuf" is commented out 
    intentionally and the src code that uses this structure should take that into account. 
**********************************************************************************************/                                        
} tAniGetPEStatsRsp, *tpAniGetPEStatsRsp;

typedef struct sAniGetRssiReq
{
    // Common for all types are requests
    tANI_U16                msgType;    // message type is same as the request type
    tANI_U16                msgLen;  // length of the entire request
    tANI_U8                 sessionId;
    tANI_U8                 staId;  
    void                    *rssiCallback;
    void                    *pDevContext; //device context
    void                    *pVosContext; //voss context
    
} tAniGetRssiReq, *tpAniGetRssiReq;

typedef struct sAniGetSnrReq
{
    // Common for all types are requests
    tANI_U16                msgType;    // message type is same as the request type
    tANI_U16                msgLen;  // length of the entire request
    tANI_U8                 sessionId;
    tANI_U8                 staId;
    void                    *snrCallback;
    void                    *pDevContext; //device context
} tAniGetSnrReq, *tpAniGetSnrReq;

#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
typedef struct sAniGetRoamRssiRsp
{
    // Common for all types are responses
    tANI_U16                msgType;    // message type is same as the request type
    tANI_U16                msgLen;  // length of the entire request, includes the pStatsBuf length too
    tANI_U8                 sessionId;
    tANI_U32                rc;         //success/failure
    tANI_U32                staId;  // Per STA stats request must contain valid
    tANI_S8                 rssi;
    void                    *rssiReq;  //rssi request backup

} tAniGetRoamRssiRsp, *tpAniGetRoamRssiRsp;

#endif

#if defined(FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_ESE_UPLOAD)

typedef struct sSirTsmIE
{
    tANI_U8      tsid;
    tANI_U8      state;
    tANI_U16     msmt_interval;
} tSirTsmIE, *tpSirTsmIE;

typedef struct sSirSmeTsmIEInd
{
    tSirTsmIE tsmIe;
    tANI_U8   sessionId;
} tSirSmeTsmIEInd, *tpSirSmeTsmIEInd;


typedef struct sAniTrafStrmMetrics
{
    tANI_U16      UplinkPktQueueDly;
    tANI_U16      UplinkPktQueueDlyHist[4];
    tANI_U32      UplinkPktTxDly;
    tANI_U16      UplinkPktLoss;
    tANI_U16      UplinkPktCount;
    tANI_U8       RoamingCount;
    tANI_U16      RoamingDly;
} tAniTrafStrmMetrics, *tpAniTrafStrmMetrics;

typedef struct sAniGetTsmStatsReq
{
    // Common for all types are requests
    tANI_U16       msgType;            // message type is same as the request type
    tANI_U16       msgLen;             // length of the entire request
    tANI_U8        staId;
    tANI_U8        tid;                // traffic id
    tSirMacAddr    bssId;
    void           *tsmStatsCallback;
    void           *pDevContext;       //device context
    void           *pVosContext;       //voss context
} tAniGetTsmStatsReq, *tpAniGetTsmStatsReq;

typedef struct sAniGetTsmStatsRsp
{
    // Common for all types are responses
    tANI_U16            msgType;      // message type is same as the request type
    tANI_U16            msgLen;       // length of the entire request, includes the pStatsBuf length too
    tANI_U8             sessionId;
    tANI_U32            rc;           //success/failure
    tANI_U32            staId;        // Per STA stats request must contain valid
    tAniTrafStrmMetrics tsmMetrics;
    void               *tsmStatsReq; //tsm stats request backup
} tAniGetTsmStatsRsp, *tpAniGetTsmStatsRsp;

typedef struct sSirEseBcnReportBssInfo
{
    tBcnReportFields  bcnReportFields;
    tANI_U8           ieLen;
    tANI_U8           *pBuf;
} tSirEseBcnReportBssInfo, *tpSirEseBcnReportBssInfo;

typedef struct sSirEseBcnReportRsp
{
    tANI_U16    measurementToken;
    tANI_U8     flag;     /* Flag to report measurement done and more data */
    tANI_U8     numBss;
    tSirEseBcnReportBssInfo bcnRepBssInfo[SIR_BCN_REPORT_MAX_BSS_DESC];
} tSirEseBcnReportRsp, *tpSirEseBcnReportRsp;

#endif /* FEATURE_WLAN_ESE || FEATURE_WLAN_ESE_UPLOAD */

/* Change country code request MSG structure */
typedef struct sAniChangeCountryCodeReq
{
    // Common for all types are requests
    tANI_U16                msgType;    // message type is same as the request type
    tANI_U16                msgLen;     // length of the entire request
    tANI_U8                 countryCode[WNI_CFG_COUNTRY_CODE_LEN];   //3 char country code
    tAniBool                countryFromUserSpace;
    tAniBool                sendRegHint;  //TRUE if we want to send hint to NL80211
    void                    *changeCCCallback;
    void                    *pDevContext; //device context
    void                    *pVosContext; //voss context

} tAniChangeCountryCodeReq, *tpAniChangeCountryCodeReq;

/* generic country code change request MSG structure */
typedef struct sAniGenericChangeCountryCodeReq
{
    // Common for all types are requests
    tANI_U16                msgType;    // message type is same as the request type
    tANI_U16                msgLen;     // length of the entire request
    tANI_U8                 countryCode[WNI_CFG_COUNTRY_CODE_LEN];   //3 char country code
    tANI_U16                domain_index;
} tAniGenericChangeCountryCodeReq, *tpAniGenericChangeCountryCodeReq;

typedef struct sAniDHCPStopInd
{
    tANI_U16                msgType;      // message type is same as the request type
    tANI_U16                msgLen;       // length of the entire request
    tANI_U8                 device_mode;  // Mode of the device(ex:STA, AP)
    tSirMacAddr             macAddr;

} tAniDHCPInd, *tpAniDHCPInd;

typedef struct sAniSummaryStatsInfo
{
    tANI_U32 retry_cnt[4];         //Total number of packets(per AC) that were successfully transmitted with retries
    tANI_U32 multiple_retry_cnt[4];//The number of MSDU packets and MMPDU frames per AC that the 802.11 
    // station successfully transmitted after more than one retransmission attempt

    tANI_U32 tx_frm_cnt[4];        //Total number of packets(per AC) that were successfully transmitted 
                                   //(with and without retries, including multi-cast, broadcast)     
    //tANI_U32 tx_fail_cnt;
    //tANI_U32 num_rx_frm_crc_err;   //Total number of received frames with CRC Error
    //tANI_U32 num_rx_frm_crc_ok;    //Total number of successfully received frames with out CRC Error
    tANI_U32 rx_frm_cnt;           //Total number of packets that were successfully received 
                                   //(after appropriate filter rules including multi-cast, broadcast)    
    tANI_U32 frm_dup_cnt;          //Total number of duplicate frames received successfully
    tANI_U32 fail_cnt[4];          //Total number packets(per AC) failed to transmit
    tANI_U32 rts_fail_cnt;         //Total number of RTS/CTS sequence failures for transmission of a packet
    tANI_U32 ack_fail_cnt;         //Total number packets failed transmit because of no ACK from the remote entity
    tANI_U32 rts_succ_cnt;         //Total number of RTS/CTS sequence success for transmission of a packet 
    tANI_U32 rx_discard_cnt;       //The sum of the receive error count and dropped-receive-buffer error count. 
                                   //HAL will provide this as a sum of (FCS error) + (Fail get BD/PDU in HW)
    tANI_U32 rx_error_cnt;         //The receive error count. HAL will provide the RxP FCS error global counter.
    tANI_U32 tx_byte_cnt;          //The sum of the transmit-directed byte count, transmit-multicast byte count 
                                   //and transmit-broadcast byte count. HAL will sum TPE UC/MC/BCAST global counters 
                                   //to provide this.
#if 0                                   
    //providing the following stats, in case of wrap around for tx_byte_cnt                                   
    tANI_U32 tx_unicast_lower_byte_cnt;
    tANI_U32 tx_unicast_upper_byte_cnt;
    tANI_U32 tx_multicast_lower_byte_cnt;
    tANI_U32 tx_multicast_upper_byte_cnt;
    tANI_U32 tx_broadcast_lower_byte_cnt;
    tANI_U32 tx_broadcast_upper_byte_cnt;
#endif

}tAniSummaryStatsInfo, *tpAniSummaryStatsInfo;

typedef enum eTxRateInfo
{
   eHAL_TX_RATE_LEGACY = 0x1,    /* Legacy rates */
   eHAL_TX_RATE_HT20   = 0x2,    /* HT20 rates */
   eHAL_TX_RATE_HT40   = 0x4,    /* HT40 rates */
   eHAL_TX_RATE_SGI    = 0x8,    /* Rate with Short guard interval */
   eHAL_TX_RATE_LGI    = 0x10,   /* Rate with Long guard interval */
   eHAL_TX_RATE_VHT20  = 0x20,   /* VHT 20 rates */
   eHAL_TX_RATE_VHT40  = 0x40,   /* VHT 40 rates */
   eHAL_TX_RATE_VHT80  = 0x80    /* VHT 80 rates */
} tTxrateinfoflags;

typedef struct sAniGlobalClassAStatsInfo
{
    tANI_U32 rx_frag_cnt;             //The number of MPDU frames received by the 802.11 station for MSDU packets 
                                     //or MMPDU frames
    tANI_U32 promiscuous_rx_frag_cnt; //The number of MPDU frames received by the 802.11 station for MSDU packets 
                                     //or MMPDU frames when a promiscuous packet filter was enabled
    //tANI_U32 rx_fcs_err;              //The number of MPDU frames that the 802.11 station received with FCS errors
    tANI_U32 rx_input_sensitivity;    //The receiver input sensitivity referenced to a FER of 8% at an MPDU length 
                                     //of 1024 bytes at the antenna connector. Each element of the array shall correspond 
                                     //to a supported rate and the order shall be the same as the supporteRates parameter.
    tANI_U32 max_pwr;                 //The maximum transmit power in dBm upto one decimal. 
                                      //for eg: if it is 10.5dBm, the value would be 105 
    //tANI_U32 default_pwr;             //The nominal transmit level used after normal power on sequence
    tANI_U32 sync_fail_cnt;           //Number of times the receiver failed to synchronize with the incoming signal 
                                     //after detecting the sync in the preamble of the transmitted PLCP protocol data unit. 
    tANI_U32 tx_rate;                //Legacy transmit rate, in units of 
                                     //500 kbit/sec, for the most 
                                     //recently transmitted frame 
    tANI_U32  mcs_index;             //mcs index for HT20 and HT40 rates
    tANI_U32  tx_rate_flags;         //to differentiate between HT20 and 
                                     //HT40 rates;  short and long guard interval

}tAniGlobalClassAStatsInfo, *tpAniGlobalClassAStatsInfo;


typedef struct sAniGlobalSecurityStats
{
    tANI_U32 rx_wep_unencrypted_frm_cnt; //The number of unencrypted received MPDU frames that the MAC layer discarded when 
                                        //the IEEE 802.11 dot11ExcludeUnencrypted management information base (MIB) object 
                                        //is enabled
    tANI_U32 rx_mic_fail_cnt;            //The number of received MSDU packets that that the 802.11 station discarded 
                                        //because of MIC failures
    tANI_U32 tkip_icv_err;               //The number of encrypted MPDU frames that the 802.11 station failed to decrypt 
                                        //because of a TKIP ICV error
    tANI_U32 aes_ccmp_format_err;        //The number of received MPDU frames that the 802.11 discarded because of an 
                                        //invalid AES-CCMP format
    tANI_U32 aes_ccmp_replay_cnt;        //The number of received MPDU frames that the 802.11 station discarded because of 
                                        //the AES-CCMP replay protection procedure
    tANI_U32 aes_ccmp_decrpt_err;        //The number of received MPDU frames that the 802.11 station discarded because of 
                                        //errors detected by the AES-CCMP decryption algorithm
    tANI_U32 wep_undecryptable_cnt;      //The number of encrypted MPDU frames received for which a WEP decryption key was 
                                        //not available on the 802.11 station
    tANI_U32 wep_icv_err;                //The number of encrypted MPDU frames that the 802.11 station failed to decrypt 
                                        //because of a WEP ICV error
    tANI_U32 rx_decrypt_succ_cnt;        //The number of received encrypted packets that the 802.11 station successfully 
                                        //decrypted
    tANI_U32 rx_decrypt_fail_cnt;        //The number of encrypted packets that the 802.11 station failed to decrypt

}tAniGlobalSecurityStats, *tpAniGlobalSecurityStats;
   
typedef struct sAniGlobalClassBStatsInfo
{
    tAniGlobalSecurityStats ucStats;
    tAniGlobalSecurityStats mcbcStats;
}tAniGlobalClassBStatsInfo, *tpAniGlobalClassBStatsInfo;

typedef struct sAniGlobalClassCStatsInfo
{
    tANI_U32 rx_amsdu_cnt;           //This counter shall be incremented for a received A-MSDU frame with the stations 
                                    //MAC address in the address 1 field or an A-MSDU frame with a group address in the 
                                    //address 1 field
    tANI_U32 rx_ampdu_cnt;           //This counter shall be incremented when the MAC receives an AMPDU from the PHY
    tANI_U32 tx_20_frm_cnt;          //This counter shall be incremented when a Frame is transmitted only on the 
                                    //primary channel
    tANI_U32 rx_20_frm_cnt;          //This counter shall be incremented when a Frame is received only on the primary channel
    tANI_U32 rx_mpdu_in_ampdu_cnt;   //This counter shall be incremented by the number of MPDUs received in the A-MPDU 
                                    //when an A-MPDU is received
    tANI_U32 ampdu_delimiter_crc_err;//This counter shall be incremented when an MPDU delimiter has a CRC error when this 
                                    //is the first CRC error in the received AMPDU or when the previous delimiter has been 
                                    //decoded correctly

}tAniGlobalClassCStatsInfo, *tpAniGlobalClassCStatsInfo;

typedef struct sAniPerStaStatsInfo
{
    tANI_U32 tx_frag_cnt[4];       //The number of MPDU frames that the 802.11 station transmitted and acknowledged 
                                  //through a received 802.11 ACK frame
    tANI_U32 tx_ampdu_cnt;         //This counter shall be incremented when an A-MPDU is transmitted 
    tANI_U32 tx_mpdu_in_ampdu_cnt; //This counter shall increment by the number of MPDUs in the AMPDU when an A-MPDU 
                                  //is transmitted

}tAniPerStaStatsInfo, *tpAniPerStaStatsInfo;

/**********************PE Statistics end*************************/



typedef struct sSirRSSIThresholds
{
#ifdef ANI_BIG_BYTE_ENDIAN
    tANI_S8   ucRssiThreshold1     : 8;
    tANI_S8   ucRssiThreshold2     : 8;
    tANI_S8   ucRssiThreshold3     : 8;
    tANI_U8   bRssiThres1PosNotify : 1;
    tANI_U8   bRssiThres1NegNotify : 1;
    tANI_U8   bRssiThres2PosNotify : 1;
    tANI_U8   bRssiThres2NegNotify : 1;
    tANI_U8   bRssiThres3PosNotify : 1;
    tANI_U8   bRssiThres3NegNotify : 1;
    tANI_U8   bReserved10          : 2;
#else
    tANI_U8   bReserved10          : 2;
    tANI_U8   bRssiThres3NegNotify : 1;
    tANI_U8   bRssiThres3PosNotify : 1;
    tANI_U8   bRssiThres2NegNotify : 1;
    tANI_U8   bRssiThres2PosNotify : 1;
    tANI_U8   bRssiThres1NegNotify : 1;
    tANI_U8   bRssiThres1PosNotify : 1;
    tANI_S8   ucRssiThreshold3     : 8;
    tANI_S8   ucRssiThreshold2     : 8;
    tANI_S8   ucRssiThreshold1     : 8;
#endif

}tSirRSSIThresholds, *tpSirRSSIThresholds;

typedef struct sSirRSSINotification
{
#ifdef ANI_BIG_BYTE_ENDIAN
    tANI_U32             bRssiThres1PosCross : 1;
    tANI_U32             bRssiThres1NegCross : 1;
    tANI_U32             bRssiThres2PosCross : 1;
    tANI_U32             bRssiThres2NegCross : 1;
    tANI_U32             bRssiThres3PosCross : 1;
    tANI_U32             bRssiThres3NegCross : 1;
    v_S7_t               avgRssi             : 8;
    tANI_U32             bReserved           : 18;
#else
    tANI_U32             bReserved           : 18;
    v_S7_t               avgRssi             : 8;
    tANI_U32             bRssiThres3NegCross : 1;
    tANI_U32             bRssiThres3PosCross : 1;
    tANI_U32             bRssiThres2NegCross : 1;
    tANI_U32             bRssiThres2PosCross : 1;
    tANI_U32             bRssiThres1NegCross : 1;
    tANI_U32             bRssiThres1PosCross : 1;
#endif
    
}tSirRSSINotification, *tpSirRSSINotification;


typedef struct sSirP2PNoaStart
{
   tANI_U32      status;
   tANI_U32      bssIdx;
} tSirP2PNoaStart, *tpSirP2PNoaStart;

typedef struct sSirTdlsInd
{
   tANI_U16      status;
   tANI_U16      assocId;
   tANI_U16      staIdx;
   tANI_U16      reasonCode;
} tSirTdlsInd, *tpSirTdlsInd;

typedef struct sSirP2PNoaAttr
{
#ifdef ANI_BIG_BYTE_ENDIAN
   tANI_U32      index :8;
   tANI_U32      oppPsFlag :1;
   tANI_U32      ctWin     :7;
   tANI_U32      rsvd1: 16;
#else
   tANI_U32      rsvd1: 16;
   tANI_U32      ctWin     :7;
   tANI_U32      oppPsFlag :1;
   tANI_U32      index :8;
#endif

#ifdef ANI_BIG_BYTE_ENDIAN
   tANI_U32       uNoa1IntervalCnt:8;
   tANI_U32       rsvd2:24;
#else
   tANI_U32       rsvd2:24;
   tANI_U32       uNoa1IntervalCnt:8;
#endif
   tANI_U32       uNoa1Duration;
   tANI_U32       uNoa1Interval;
   tANI_U32       uNoa1StartTime;

#ifdef ANI_BIG_BYTE_ENDIAN
   tANI_U32       uNoa2IntervalCnt:8;
   tANI_U32       rsvd3:24;
#else
   tANI_U32       rsvd3:24;
   tANI_U32       uNoa2IntervalCnt:8;
#endif
   tANI_U32       uNoa2Duration;
   tANI_U32       uNoa2Interval;
   tANI_U32       uNoa2StartTime;
} tSirP2PNoaAttr, *tpSirP2PNoaAttr;

typedef __ani_attr_pre_packed struct sSirTclasInfo
{
    tSirMacTclasIE   tclas;
    tANI_U8               version; // applies only for classifier type ip
    __ani_attr_pre_packed union {
        tSirMacTclasParamEthernet eth;
        tSirMacTclasParamIPv4     ipv4;
        tSirMacTclasParamIPv6     ipv6;
        tSirMacTclasParam8021dq   t8021dq;
    }__ani_attr_packed tclasParams;
} __ani_attr_packed tSirTclasInfo;


#if defined(FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_ESE_UPLOAD)
#define TSRS_11AG_RATE_6MBPS   0xC
#define TSRS_11B_RATE_5_5MBPS  0xB

typedef struct sSirMacESETSRSIE
{
    tANI_U8      tsid;
    tANI_U8      rates[8];
} tSirMacESETSRSIE;

typedef struct sSirMacESETSMIE
{
    tANI_U8      tsid;
    tANI_U8      state;
    tANI_U16     msmt_interval;
} tSirMacESETSMIE;

typedef struct sTSMStats
{
    tANI_U8           tid;
    tSirMacAddr       bssId;
    tTrafStrmMetrics  tsmMetrics;
} tTSMStats, *tpTSMStats;

typedef struct sEseTSMContext
{
   tANI_U8           tid;
   tSirMacESETSMIE   tsmInfo;
   tTrafStrmMetrics  tsmMetrics;
} tEseTSMContext, *tpEseTSMContext;

typedef struct sEsePEContext
{
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
   tEseMeasReq       curMeasReq;
#endif
   tEseTSMContext    tsm;
} tEsePEContext, *tpEsePEContext;


#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */


typedef struct sSirAddtsReqInfo
{
    tANI_U8               dialogToken;
    tSirMacTspecIE   tspec;

    tANI_U8               numTclas; // number of Tclas elements
    tSirTclasInfo    tclasInfo[SIR_MAC_TCLASIE_MAXNUM];
    tANI_U8               tclasProc;
#if defined(FEATURE_WLAN_ESE)
    tSirMacESETSRSIE      tsrsIE;
    tANI_U8               tsrsPresent:1;
#endif
    tANI_U8               wmeTspecPresent:1;
    tANI_U8               wsmTspecPresent:1;
    tANI_U8               lleTspecPresent:1;
    tANI_U8               tclasProcPresent:1;
} tSirAddtsReqInfo, *tpSirAddtsReqInfo;

typedef struct sSirAddtsRspInfo
{
    tANI_U8                 dialogToken;
    tSirMacStatusCodes status;
    tSirMacTsDelayIE   delay;

    tSirMacTspecIE     tspec;
    tANI_U8                 numTclas; // number of Tclas elements
    tSirTclasInfo      tclasInfo[SIR_MAC_TCLASIE_MAXNUM];
    tANI_U8                 tclasProc;
    tSirMacScheduleIE  schedule;
#if defined(FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_ESE_UPLOAD)
    tSirMacESETSMIE    tsmIE;
    tANI_U8                 tsmPresent:1;
#endif
    tANI_U8                 wmeTspecPresent:1;
    tANI_U8                 wsmTspecPresent:1;
    tANI_U8                 lleTspecPresent:1;
    tANI_U8                 tclasProcPresent:1;
    tANI_U8                 schedulePresent:1;
} tSirAddtsRspInfo, *tpSirAddtsRspInfo;

typedef struct sSirDeltsReqInfo
{
    tSirMacTSInfo      tsinfo;
    tSirMacTspecIE     tspec;
    tANI_U8                 wmeTspecPresent:1;
    tANI_U8                 wsmTspecPresent:1;
    tANI_U8                 lleTspecPresent:1;
} tSirDeltsReqInfo, *tpSirDeltsReqInfo;

/// Add a tspec as defined
typedef struct sSirAddtsReq
{
    tANI_U16                messageType; // eWNI_SME_ADDTS_REQ
    tANI_U16                length;
    tANI_U8                 sessionId;  //Session ID
    tANI_U16                transactionId;
    tSirMacAddr             bssId;      //BSSID
    tANI_U32                timeout; // in ms
    tANI_U8                 rspReqd;
    tSirAddtsReqInfo        req;
} tSirAddtsReq, *tpSirAddtsReq;

typedef struct sSirAddtsRsp
{
    tANI_U16                messageType; // eWNI_SME_ADDTS_RSP
    tANI_U16                length;
    tANI_U8                 sessionId;  // sme sessionId  Added for BT-AMP support 
    tANI_U16                transactionId; //sme transaction Id Added for BT-AMP Support 
    tANI_U32                rc;          // return code
    tSirAddtsRspInfo        rsp;
} tSirAddtsRsp, *tpSirAddtsRsp;

typedef struct sSirDeltsReq
{
    tANI_U16                messageType; // eWNI_SME_DELTS_REQ
    tANI_U16                length;
    tANI_U8                 sessionId;//Session ID
    tANI_U16                transactionId;
    tSirMacAddr             bssId;  //BSSID
    tANI_U16                aid;  // use 0 if macAddr is being specified
    tANI_U8                 macAddr[6]; // only on AP to specify the STA
    tANI_U8                 rspReqd;
    tSirDeltsReqInfo        req;
} tSirDeltsReq, *tpSirDeltsReq;

typedef struct sSirDeltsRsp
{
    tANI_U16                messageType; // eWNI_SME_DELTS_RSP
    tANI_U16                length;
    tANI_U8                 sessionId;  // sme sessionId  Added for BT-AMP support 
    tANI_U16                transactionId; //sme transaction Id Added for BT-AMP Support 
    tANI_U32                rc;
    tANI_U16                aid;  // use 0 if macAddr is being specified
    tANI_U8                 macAddr[6]; // only on AP to specify the STA
    tSirDeltsReqInfo        rsp;
} tSirDeltsRsp, *tpSirDeltsRsp;

#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)

#define SIR_QOS_NUM_TSPEC_MAX 2
#define SIR_QOS_NUM_AC_MAX 4

typedef struct sSirAggrQosReqInfo
{
    tANI_U16 tspecIdx;
    tSirAddtsReqInfo aggrAddTsInfo[SIR_QOS_NUM_AC_MAX];
}tSirAggrQosReqInfo, *tpSirAggrQosReqInfo;

typedef struct sSirAggrQosReq
{
    tANI_U16                messageType; // eWNI_SME_ADDTS_REQ
    tANI_U16                length;
    tANI_U8                 sessionId;  //Session ID
    tANI_U16                transactionId;
    tSirMacAddr             bssId;      //BSSID
    tANI_U32                timeout; // in ms
    tANI_U8                 rspReqd;
    tSirAggrQosReqInfo      aggrInfo;
}tSirAggrQosReq, *tpSirAggrQosReq;

typedef struct sSirAggrQosRspInfo
{
    tANI_U16                tspecIdx;
    tSirAddtsRspInfo        aggrRsp[SIR_QOS_NUM_AC_MAX];
} tSirAggrQosRspInfo, *tpSirAggrQosRspInfo;

typedef struct sSirAggrQosRsp
{
    tANI_U16                messageType;
    tANI_U16                length;
    tANI_U8                 sessionId;
    tSirAggrQosRspInfo      aggrInfo;
} tSirAggrQosRsp, *tpSirAggrQosRsp;

#endif/*WLAN_FEATURE_VOWIFI_11R || FEATURE_WLAN_ESE*/

typedef struct sSirSetTxPowerReq
{
    tANI_U16       messageType;
    tANI_U16       length;
    tSirMacAddr    bssId;
    tANI_U8        mwPower;
    tANI_U8        bssIdx;
} tSirSetTxPowerReq, *tpSirSetTxPowerReq;

typedef struct sSirSetTxPowerRsp
{
    tANI_U16            messageType;
    tANI_U16        length;
    tANI_U32        status;
} tSirSetTxPowerRsp, *tpSirSetTxPowerRsp;

typedef struct sSirGetTxPowerReq
{
    tANI_U16    messageType;
    tANI_U16    length;
    tANI_U16    staid;
} tSirGetTxPowerReq, *tpSirGetTxPowerReq;

typedef struct sSirGetTxPowerRsp
{
    tANI_U16            messageType;
    tANI_U16            length; // length of the entire request
    tANI_U32            power;  // units of milliwatts
    tANI_U32            status;
} tSirGetTxPowerRsp, *tpSirGetTxPowerRsp;


typedef tANI_U32 tSirMacNoise[3];

typedef struct sSirGetNoiseRsp 
{
    tANI_U16            messageType;
    tANI_U16            length; 
    tSirMacNoise        noise;
} tSirGetNoiseRsp, *tpSirGetNoiseRsp;

typedef struct sSirQosMapSet
{
    tANI_U8      present;
    tANI_U8      num_dscp_exceptions;
    tANI_U8      dscp_exceptions[21][2];
    tANI_U8      dscp_range[8][2];
} tSirQosMapSet, *tpSirQosMapSet;

//
// PMC --> PE --> HAL
// Power save configuration parameters
//
typedef struct sSirPowerSaveCfg
{
    tANI_U16    listenInterval;
   
    /* Number of consecutive missed beacons before 
     * hardware generates an interrupt to wake up 
     * the host. In units of listen interval.
     */
    tANI_U32 HeartBeatCount;

    /* specifies which beacons are to be forwarded
     * to host when beacon filtering is enabled.
     * In units of listen interval.
     */
    tANI_U32    nthBeaconFilter;

    /* Maximum number of PS-Poll send before 
     * firmware sends data null with PM set to 0.
     */
    tANI_U32    maxPsPoll;                                                 

    /* If the average RSSI value falls below the 
     * minRssiThreshold, then FW will send an 
     * interrupt to wake up the host. 
     */
    tANI_U32    minRssiThreshold;                                       

    /* Number of beacons for which firmware will 
     * collect the RSSI values and compute the average.
     */
    tANI_U8     numBeaconPerRssiAverage;                        

    /* FW collects the RSSI stats for this period
     * in BMPS mode.  
     */
    tANI_U8     rssiFilterPeriod;

    // Enabling/disabling broadcast frame filter feature
    tANI_U8     broadcastFrameFilter;    

    // Enabling/disabling the ignore DTIM feature
    tANI_U8     ignoreDtim;

    /* The following configuration parameters are kept
     * in order to be backward compatible for Gen5. 
     * These will NOT be used for Gen6 Libra chip
     */
    tBeaconForwarding beaconFwd;
    tANI_U16 nthBeaconFwd;
    tANI_U8 fEnablePwrSaveImmediately;
    tANI_U8 fPSPoll;

    // Enabling/disabling Beacon Early Termination feature
    tANI_U8     fEnableBeaconEarlyTermination;    
    tANI_U8     bcnEarlyTermWakeInterval;    

}tSirPowerSaveCfg, *tpSirPowerSaveCfg;

/* Reason code for requesting Full Power. This reason code is used by 
   any module requesting full power from PMC and also by PE when it
   sends the eWNI_PMC_EXIT_BMPS_IND to PMC*/
typedef enum eRequestFullPowerReason
{
   eSME_MISSED_BEACON_IND_RCVD,    /* PE received a MAX_MISSED_BEACON_IND */
   eSME_BMPS_STATUS_IND_RCVD,      /* PE received a SIR_HAL_BMPS_STATUS_IND */
   eSME_BMPS_MODE_DISABLED,        /* BMPS mode was disabled by HDD in SME */
   eSME_LINK_DISCONNECTED_BY_HDD,  /* Link has been disconnected requested by HDD */
   eSME_LINK_DISCONNECTED_BY_OTHER,/* Disconnect due to linklost or requested by peer */
   eSME_FULL_PWR_NEEDED_BY_HDD,    /* HDD request full power for some reason */
   eSME_FULL_PWR_NEEDED_BY_BAP,    /* BAP request full power for BT_AMP */
   eSME_FULL_PWR_NEEDED_BY_CSR,    /* CSR requests full power */
   eSME_FULL_PWR_NEEDED_BY_QOS,    /* QOS requests full power */
   eSME_FULL_PWR_NEEDED_BY_CHANNEL_SWITCH, /* channel switch request full power*/
#ifdef FEATURE_WLAN_TDLS
   eSME_FULL_PWR_NEEDED_BY_TDLS_PEER_SETUP, /* TDLS peer setup*/
#endif
   eSME_REASON_OTHER               /* No specific reason. General reason code */ 
} tRequestFullPowerReason, tExitBmpsReason;



//This is sent alongwith eWNI_PMC_EXIT_BMPS_REQ message
typedef struct sExitBmpsInfo
{
   tExitBmpsReason exitBmpsReason;  /*Reason for exiting BMPS */
}tExitBmpsInfo, *tpExitBmpsInfo;


// MAC SW --> SME
// Message indicating to SME to exit BMPS sleep mode
typedef struct sSirSmeExitBmpsInd
{
    tANI_U16  mesgType;               /* eWNI_PMC_EXIT_BMPS_IND */
    tANI_U16  mesgLen;
    tSirResultCodes  statusCode;
    tExitBmpsReason  exitBmpsReason;  /*Reason for exiting BMPS */

} tSirSmeExitBmpsInd, *tpSirSmeExitBmpsInd;


//
// HDD -> LIM
// tSirMsgQ.type = eWNI_SME_DEL_BA_PEER_IND
// tSirMsgQ.reserved = 0
// tSirMsgQ.body = instance of tDelBAParams
//
typedef struct sSmeDelBAPeerInd
{
    // Message Type
    tANI_U16 mesgType;

    tSirMacAddr bssId;//BSSID 

    // Message Length
    tANI_U16 mesgLen;

    // Station Index
    tANI_U16 staIdx;

    // TID for which the BA session is being deleted
    tANI_U8 baTID;

    // DELBA direction
    // eBA_INITIATOR - Originator
    // eBA_RECEIPIENT - Recipient
    tANI_U8 baDirection;
} tSmeDelBAPeerInd, *tpSmeDelBAPeerInd;

typedef struct sSmeIbssPeerInd
{
    tANI_U16    mesgType;
    tANI_U16    mesgLen;
    tANI_U8     sessionId;

    tSirMacAddr peerAddr;
    tANI_U16    staId;

    /*The DPU signatures will be sent eventually to TL to help it determine the 
      association to which a packet belongs to*/
    /*Unicast DPU signature*/
    tANI_U8            ucastSig;

    /*Broadcast DPU signature*/
    tANI_U8            bcastSig;

    //Beacon will be appended for new Peer indication.
}tSmeIbssPeerInd, *tpSmeIbssPeerInd;

typedef struct sSirIbssPeerInactivityInd
{
   tANI_U8       bssIdx;
   tANI_U8       staIdx;
   tSirMacAddr   peerAddr;
}tSirIbssPeerInactivityInd, *tpSirIbssPeerInactivityInd;


typedef struct sLimScanChn
{
    tANI_U16 numTimeScan;   //how many time this channel is scan
    tANI_U8 channelId;
}tLimScanChn;

typedef struct sSmeGetScanChnRsp
{
    // Message Type
    tANI_U16 mesgType;
    // Message Length
    tANI_U16 mesgLen;
    tANI_U8   sessionId;
    tANI_U8 numChn;
    tLimScanChn scanChn[1];
} tSmeGetScanChnRsp, *tpSmeGetScanChnRsp;

typedef struct sLimScanChnInfo
{
    tANI_U8 numChnInfo;     //number of channels in scanChn
    tLimScanChn scanChn[SIR_MAX_SUPPORTED_CHANNEL_LIST];
}tLimScanChnInfo;

typedef struct sSirSmeGetAssocSTAsReq
{
    tANI_U16    messageType;    // eWNI_SME_GET_ASSOC_STAS_REQ
    tANI_U16    length;    
    tSirMacAddr bssId;          // BSSID
    tANI_U16    modId;
    void        *pUsrContext;
    void        *pSapEventCallback;
    void        *pAssocStasArray;// Pointer to allocated memory passed in WLANSAP_GetAssocStations API
} tSirSmeGetAssocSTAsReq, *tpSirSmeGetAssocSTAsReq;

typedef struct sSmeMaxAssocInd
{
    tANI_U16    mesgType;    // eWNI_SME_MAX_ASSOC_EXCEEDED
    tANI_U16    mesgLen;    
    tANI_U8     sessionId;    
    tSirMacAddr peerMac;     // the new peer that got rejected due to softap max assoc limit reached
} tSmeMaxAssocInd, *tpSmeMaxAssocInd;

/*--------------------------------------------------------------------*/
/* BootLoader message definition                                      */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/* FW image size                                                      */
/*--------------------------------------------------------------------*/
#define SIR_FW_IMAGE_SIZE            146332


#define SIR_BOOT_MODULE_ID           1

#define SIR_BOOT_SETUP_IND           ((SIR_BOOT_MODULE_ID << 8) | 0x11)
#define SIR_BOOT_POST_RESULT_IND     ((SIR_BOOT_MODULE_ID << 8) | 0x12)
#define SIR_BOOT_DNLD_RESULT_IND     ((SIR_BOOT_MODULE_ID << 8) | 0x13)
#define SIR_BOOT_DNLD_DEV_REQ        ((SIR_BOOT_MODULE_ID << 8) | 0x41)
#define SIR_BOOT_DNLD_DEV_RSP        ((SIR_BOOT_MODULE_ID << 8) | 0x81)
#define SIR_BOOT_DNLD_REQ            ((SIR_BOOT_MODULE_ID << 8) | 0x42)
#define SIR_BOOT_DNLD_RSP            ((SIR_BOOT_MODULE_ID << 8) | 0x82)

/*--------------------------------------------------------------------*/
/* Bootloader message syntax                                          */
/*--------------------------------------------------------------------*/

// Message header
#define SIR_BOOT_MB_HEADER                 0
#define SIR_BOOT_MB_HEADER2                1

#define SIR_BOOT_MSG_HDR_MASK              0xffff0000
#define SIR_BOOT_MSG_LEN_MASK              0x0000ffff

// BOOT_SETUP_IND parameter indices
#define SIR_BOOT_SETUP_IND_MBADDR          2
#define SIR_BOOT_SETUP_IND_MBSIZE          3
#define SIR_BOOT_SETUP_IND_MEMOPT          4
#define SIR_BOOT_SETUP_IND_LEN             \
                                      ((SIR_BOOT_SETUP_IND_MEMOPT+1)<<2)

// BOOT_POST_RESULT_IND parameter indices
#define SIR_BOOT_POST_RESULT_IND_RES       2
#define SIR_BOOT_POST_RESULT_IND_LEN       \
                                  ((SIR_BOOT_POST_RESULT_IND_RES+1)<<2)

#define SIR_BOOT_POST_RESULT_IND_SUCCESS       1
#define SIR_BOOT_POST_RESULT_IND_MB_FAILED     2
#define SIR_BOOT_POST_RESULT_IND_SDRAM_FAILED  3
#define SIR_BOOT_POST_RESULT_IND_ESRAM_FAILED  4


// BOOT_DNLD_RESULT_IND parameter indices
#define SIR_BOOT_DNLD_RESULT_IND_RES       2
#define SIR_BOOT_DNLD_RESULT_IND_LEN       \
                                   ((SIR_BOOT_DNLD_RESULT_IND_RES+1)<<2)

#define SIR_BOOT_DNLD_RESULT_IND_SUCCESS   1
#define SIR_BOOT_DNLD_RESULT_IND_HDR_ERR   2
#define SIR_BOOT_DNLD_RESULT_IND_ERR       3

// BOOT_DNLD_DEV_REQ
#define SIR_BOOT_DNLD_DEV_REQ_SDRAMSIZE    2
#define SIR_BOOT_DNLD_DEV_REQ_FLASHSIZE    3
#define SIR_BOOT_DNLD_DEV_REQ_LEN          \
                                 ((SIR_BOOT_DNLD_DEV_REQ_FLASHSIZE+1)<<2)

// BOOT_DNLD_DEV_RSP
#define SIR_BOOT_DNLD_DEV_RSP_DEVTYPE      2
#define SIR_BOOT_DNLD_DEV_RSP_LEN          \
                                   ((SIR_BOOT_DNLD_DEV_RSP_DEVTYPE+1)<<2)

#define SIR_BOOT_DNLD_DEV_RSP_SRAM         1
#define SIR_BOOT_DNLD_DEV_RSP_FLASH        2

// BOOT_DNLD_REQ
#define SIR_BOOT_DNLD_REQ_OFFSET           2
#define SIR_BOOT_DNLD_REQ_WRADDR           3
#define SIR_BOOT_DNLD_REQ_SIZE             4
#define SIR_BOOT_DNLD_REQ_LEN              ((SIR_BOOT_DNLD_REQ_SIZE+1)<<2)

// BOOT_DNLD_RSP
#define SIR_BOOT_DNLD_RSP_SIZE             2
#define SIR_BOOT_DNLD_RSP_LEN              ((SIR_BOOT_DNLD_RSP_SIZE+1)<<2)


// board capabilities fields are defined here.
typedef __ani_attr_pre_packed struct sSirBoardCapabilities
{
#ifndef ANI_LITTLE_BIT_ENDIAN
    tANI_U32 concat:1;        // 0 - Concat is not supported, 1 - Concat is supported
    tANI_U32 compression:1;   // 0 - Compression is not supported, 1 - Compression is supported
    tANI_U32 chnlBonding:1;   // 0 - Channel Bonding is not supported, 1 - Channel Bonding is supported
    tANI_U32 reverseFCS:1;    // 0 - Reverse FCS is not supported, 1 - Reverse FCS is supported
    tANI_U32 rsvd1:2;
    // (productId derives sub-category in the following three families)
    tANI_U32 cbFamily:1;      // 0 - Not CB family, 1 - Cardbus
    tANI_U32 apFamily:1;      // 0 - Not AP family, 1 - AP
    tANI_U32 mpciFamily:1;    // 0 - Not MPCI family, 1 - MPCI
    tANI_U32 bgOnly:1;        // 0 - default a/b/g; 1 - b/g only
    tANI_U32 bbChipVer:4;     // Baseband chip version
    tANI_U32 loType:2;        // 0 = no LO, 1 = SILABS, 2 = ORION
    tANI_U32 radioOn:2;       // Not supported is 3 or 2, 0 = Off and 1 = On
    tANI_U32 nReceivers:2;    // 0 based.
    tANI_U32 nTransmitters:1; // 0 = 1 transmitter, 1 = 2 transmitters
    tANI_U32 sdram:1;         // 0 = no SDRAM, 1 = SDRAM
    tANI_U32 rsvd:1;
    tANI_U32 extVsIntAnt:1;   // 0 = ext antenna, 1 = internal antenna
#else

    tANI_U32 extVsIntAnt:1;   // 0 = ext antenna, 1 = internal antenna
    tANI_U32 rsvd:1;
    tANI_U32 sdram:1;         // 0 = no SDRAM, 1 = SDRAM
    tANI_U32 nTransmitters:1; // 0 = 1 transmitter, 1 = 2 transmitters
    tANI_U32 nReceivers:2;    // 0 based.
    tANI_U32 radioOn:2;       // Not supported is 3 or 2, 0 = Off and 1 = On
    tANI_U32 loType:2;        // 0 = no LO, 1 = SILABS, 2 = ORION
    tANI_U32 bbChipVer:4;     // Baseband chip version
    tANI_U32 bgOnly:1;        // 0 - default a/b/g; 1 - b/g only
    // (productId derives sub-category in the following three families)
    tANI_U32 mpciFamily:1;    // 0 - Not MPCI family, 1 - MPCI
    tANI_U32 apFamily:1;      // 0 - Not AP family, 1 - AP
    tANI_U32 cbFamily:1;      // 0 - Not CB family, 1 - Cardbus
    tANI_U32 rsvd1:2;
    tANI_U32 reverseFCS:1;    // 0 - Reverse FCS is not supported, 1 - Reverse FCS is supported
    tANI_U32 chnlBonding:1;   // 0 - Channel Bonding is not supported, 1 - Channel Bonding is supported
    tANI_U32 compression:1;   // 0 - Compression is not supported, 1 - Compression is supported
    tANI_U32 concat:1;        // 0 - Concat is not supported, 1 - Concat is supported
#endif
} __ani_attr_packed  tSirBoardCapabilities, *tpSirBoardCapabilities;

# define ANI_BCAP_EXT_VS_INT_ANT_MASK   0x1
# define ANI_BCAP_EXT_VS_INT_ANT_OFFSET 0

# define ANI_BCAP_GAL_ON_BOARD_MASK     0x2
# define ANI_BCAP_GAL_ON_BOARD_OFFSET   1

# define ANI_BCAP_SDRAM_MASK            0x4
# define ANI_BCAP_SDRAM_OFFSET          2

# define ANI_BCAP_NUM_TRANSMITTERS_MASK   0x8
# define ANI_BCAP_NUM_TRANSMITTERS_OFFSET 3

# define ANI_BCAP_NUM_RECEIVERS_MASK    0x30
# define ANI_BCAP_NUM_RECEIVERS_OFFSET  4

# define ANI_BCAP_RADIO_ON_MASK         0xC0
# define ANI_BCAP_RADIO_ON_OFFSET       6

# define ANI_BCAP_LO_TYPE_MASK          0x300
# define ANI_BCAP_LO_TYPE_OFFSET        8

# define ANI_BCAP_BB_CHIP_VER_MASK      0xC00
# define ANI_BCAP_BB_CHIP_VER_OFFSET    10

# define ANI_BCAP_CYG_DATE_CODE_MASK    0xFF000
# define ANI_BCAP_CYG_DATE_CODE_OFFSET  12

# define ANI_BCAP_RADIO_OFF              0
# define ANI_BCAP_RADIO_ON               1
# define ANI_BCAP_RADIO_ON_NOT_SUPPORTED 3


/// WOW related structures
// SME -> PE <-> HAL
#define SIR_WOWL_BCAST_PATTERN_MAX_SIZE 128
#define SIR_WOWL_BCAST_MAX_NUM_PATTERNS 16

// SME -> PE -> HAL - This is to add WOWL BCAST wake-up pattern. 
// SME/HDD maintains the list of the BCAST wake-up patterns.
// This is a pass through message for PE
typedef struct sSirWowlAddBcastPtrn
{
    tANI_U8  ucPatternId;           // Pattern ID
    // Pattern byte offset from beginning of the 802.11 packet to start of the
    // wake-up pattern
    tANI_U8  ucPatternByteOffset;   
    tANI_U8  ucPatternSize;         // Non-Zero Pattern size
    tANI_U8  ucPattern[SIR_WOWL_BCAST_PATTERN_MAX_SIZE]; // Pattern
    tANI_U8  ucPatternMaskSize;     // Non-zero pattern mask size
    tANI_U8  ucPatternMask[SIR_WOWL_BCAST_PATTERN_MAX_SIZE]; // Pattern mask
    // Extra pattern data beyond 128 bytes
    tANI_U8  ucPatternExt[SIR_WOWL_BCAST_PATTERN_MAX_SIZE]; // Extra Pattern
    tANI_U8  ucPatternMaskExt[SIR_WOWL_BCAST_PATTERN_MAX_SIZE]; // Extra Pattern mask
    tSirMacAddr    bssId;           // BSSID
} tSirWowlAddBcastPtrn, *tpSirWowlAddBcastPtrn;


// SME -> PE -> HAL - This is to delete WOWL BCAST wake-up pattern. 
// SME/HDD maintains the list of the BCAST wake-up patterns.
// This is a pass through message for PE
typedef struct sSirWowlDelBcastPtrn
{
    /* Pattern ID of the wakeup pattern to be deleted */
    tANI_U8  ucPatternId;
    tSirMacAddr    bssId;           // BSSID
}tSirWowlDelBcastPtrn, *tpSirWowlDelBcastPtrn;


// SME->PE: Enter WOWLAN parameters 
typedef struct sSirSmeWowlEnterParams
{
    /* Enables/disables magic packet filtering */
    tANI_U8   ucMagicPktEnable; 

    /* Magic pattern */
    tSirMacAddr magicPtrn;

    /* Enables/disables packet pattern filtering */
    tANI_U8   ucPatternFilteringEnable; 

#ifdef WLAN_WAKEUP_EVENTS
    /* This configuration directs the WoW packet filtering to look for EAP-ID
     * requests embedded in EAPOL frames and use this as a wake source.
     */
    tANI_U8   ucWoWEAPIDRequestEnable;

    /* This configuration directs the WoW packet filtering to look for EAPOL-4WAY
     * requests and use this as a wake source.
     */
    tANI_U8   ucWoWEAPOL4WayEnable;

    /* This configuration allows a host wakeup on an network scan offload match.
     */
    tANI_U8   ucWowNetScanOffloadMatch;

    /* This configuration allows a host wakeup on any GTK rekeying error.
     */
    tANI_U8   ucWowGTKRekeyError;

    /* This configuration allows a host wakeup on BSS connection loss.
     */
    tANI_U8   ucWoWBSSConnLoss;
#endif // WLAN_WAKEUP_EVENTS

    tSirMacAddr bssId;
} tSirSmeWowlEnterParams, *tpSirSmeWowlEnterParams;


// PE<->HAL: Enter WOWLAN parameters 
typedef struct sSirHalWowlEnterParams
{
    /* Enables/disables magic packet filtering */
    tANI_U8   ucMagicPktEnable; 

    /* Magic pattern */
    tSirMacAddr magicPtrn;

    /* Enables/disables packet pattern filtering in firmware. 
       Enabling this flag enables broadcast pattern matching 
       in Firmware. If unicast pattern matching is also desired,  
       ucUcastPatternFilteringEnable flag must be set tot true 
       as well 
    */
    tANI_U8   ucPatternFilteringEnable;

    /* Enables/disables unicast packet pattern filtering. 
       This flag specifies whether we want to do pattern match 
       on unicast packets as well and not just broadcast packets. 
       This flag has no effect if the ucPatternFilteringEnable 
       (main controlling flag) is set to false
    */
    tANI_U8   ucUcastPatternFilteringEnable;                     

    /* This configuration is valid only when magicPktEnable=1. 
     * It requests hardware to wake up when it receives the 
     * Channel Switch Action Frame.
     */
    tANI_U8   ucWowChnlSwitchRcv;

    /* This configuration is valid only when magicPktEnable=1. 
     * It requests hardware to wake up when it receives the 
     * Deauthentication Frame. 
     */
    tANI_U8   ucWowDeauthRcv;

    /* This configuration is valid only when magicPktEnable=1. 
     * It requests hardware to wake up when it receives the 
     * Disassociation Frame. 
     */
    tANI_U8   ucWowDisassocRcv;

    /* This configuration is valid only when magicPktEnable=1. 
     * It requests hardware to wake up when it has missed
     * consecutive beacons. This is a hardware register
     * configuration (NOT a firmware configuration). 
     */
    tANI_U8   ucWowMaxMissedBeacons;

    /* This configuration is valid only when magicPktEnable=1. 
     * This is a timeout value in units of microsec. It requests
     * hardware to unconditionally wake up after it has stayed
     * in WoWLAN mode for some time. Set 0 to disable this feature.      
     */
    tANI_U8   ucWowMaxSleepUsec;

#ifdef WLAN_WAKEUP_EVENTS
    /* This configuration directs the WoW packet filtering to look for EAP-ID
     * requests embedded in EAPOL frames and use this as a wake source.
     */
    tANI_U8   ucWoWEAPIDRequestEnable;

    /* This configuration directs the WoW packet filtering to look for EAPOL-4WAY
     * requests and use this as a wake source.
     */
    tANI_U8   ucWoWEAPOL4WayEnable;

    /* This configuration allows a host wakeup on an network scan offload match.
     */
    tANI_U8   ucWowNetScanOffloadMatch;

    /* This configuration allows a host wakeup on any GTK rekeying error.
     */
    tANI_U8   ucWowGTKRekeyError;

    /* This configuration allows a host wakeup on BSS connection loss.
     */
    tANI_U8   ucWoWBSSConnLoss;
#endif // WLAN_WAKEUP_EVENTS

    /* Status code to be filled by HAL when it sends
     * SIR_HAL_WOWL_ENTER_RSP to PE. 
     */  
    eHalStatus  status;

   /*BSSID to find the current session
      */
    tANI_U8  bssIdx;
} tSirHalWowlEnterParams, *tpSirHalWowlEnterParams;

// PE<->HAL: Exit WOWLAN parameters 
typedef struct sSirHalWowlExitParams
{
    /* Status code to be filled by HAL when it sends
     * SIR_HAL_WOWL_EXIT_RSP to PE. 
     */  
    eHalStatus  status;

   /*BSSIDX to find the current session
      */
    tANI_U8  bssIdx;
} tSirHalWowlExitParams, *tpSirHalWowlExitParams;


#define SIR_MAX_NAME_SIZE 64
#define SIR_MAX_TEXT_SIZE 32

typedef struct sSirName {
    v_U8_t num_name;
    v_U8_t name[SIR_MAX_NAME_SIZE];
} tSirName;

typedef struct sSirText {
    v_U8_t num_text;
    v_U8_t text[SIR_MAX_TEXT_SIZE];
} tSirText;


#define SIR_WPS_PROBRSP_VER_PRESENT    0x00000001
#define SIR_WPS_PROBRSP_STATE_PRESENT    0x00000002
#define SIR_WPS_PROBRSP_APSETUPLOCK_PRESENT    0x00000004
#define SIR_WPS_PROBRSP_SELECTEDREGISTRA_PRESENT    0x00000008
#define SIR_WPS_PROBRSP_DEVICEPASSWORDID_PRESENT    0x00000010
#define SIR_WPS_PROBRSP_SELECTEDREGISTRACFGMETHOD_PRESENT    0x00000020
#define SIR_WPS_PROBRSP_RESPONSETYPE_PRESENT    0x00000040
#define SIR_WPS_PROBRSP_UUIDE_PRESENT    0x00000080
#define SIR_WPS_PROBRSP_MANUFACTURE_PRESENT    0x00000100
#define SIR_WPS_PROBRSP_MODELNAME_PRESENT    0x00000200
#define SIR_WPS_PROBRSP_MODELNUMBER_PRESENT    0x00000400
#define SIR_WPS_PROBRSP_SERIALNUMBER_PRESENT    0x00000800
#define SIR_WPS_PROBRSP_PRIMARYDEVICETYPE_PRESENT    0x00001000
#define SIR_WPS_PROBRSP_DEVICENAME_PRESENT    0x00002000
#define SIR_WPS_PROBRSP_CONFIGMETHODS_PRESENT    0x00004000
#define SIR_WPS_PROBRSP_RF_BANDS_PRESENT    0x00008000


typedef struct sSirWPSProbeRspIE {
   v_U32_t  FieldPresent;
   v_U32_t  Version;           // Version. 0x10 = version 1.0, 0x11 = etc.
   v_U32_t  wpsState;          // 1 = unconfigured, 2 = configured.    
   v_BOOL_t APSetupLocked;     // Must be included if value is TRUE
   v_BOOL_t SelectedRegistra;  //BOOL:  indicates if the user has recently activated a Registrar to add an Enrollee.
   v_U16_t  DevicePasswordID;  // Device Password ID
   v_U16_t  SelectedRegistraCfgMethod; // Selected Registrar config method
   v_U8_t   ResponseType;      // Response type
   v_U8_t   UUID_E[16];         // Unique identifier of the AP.
   tSirName   Manufacture;
   tSirText   ModelName;
   tSirText   ModelNumber;
   tSirText  SerialNumber;
   v_U32_t  PrimaryDeviceCategory ; // Device Category ID: 1Computer, 2Input Device, ...
   v_U8_t   PrimaryDeviceOUI[4] ; // Vendor specific OUI for Device Sub Category
   v_U32_t  DeviceSubCategory ; // Device Sub Category ID: 1-PC, 2-Server if Device Category ID is computer
   tSirText DeviceName;
   v_U16_t  ConfigMethod;     // Configuaration method
   v_U8_t   RFBand;           // RF bands available on the AP
} tSirWPSProbeRspIE;

#define SIR_WPS_BEACON_VER_PRESENT    0x00000001
#define SIR_WPS_BEACON_STATE_PRESENT    0x00000002
#define SIR_WPS_BEACON_APSETUPLOCK_PRESENT    0x00000004
#define SIR_WPS_BEACON_SELECTEDREGISTRA_PRESENT    0x00000008
#define SIR_WPS_BEACON_DEVICEPASSWORDID_PRESENT    0x00000010
#define SIR_WPS_BEACON_SELECTEDREGISTRACFGMETHOD_PRESENT    0x00000020
#define SIR_WPS_BEACON_UUIDE_PRESENT    0x00000080
#define SIR_WPS_BEACON_RF_BANDS_PRESENT    0x00000100

typedef struct sSirWPSBeaconIE {
   v_U32_t  FieldPresent;
   v_U32_t  Version;           // Version. 0x10 = version 1.0, 0x11 = etc.
   v_U32_t  wpsState;          // 1 = unconfigured, 2 = configured.    
   v_BOOL_t APSetupLocked;     // Must be included if value is TRUE
   v_BOOL_t SelectedRegistra;  //BOOL:  indicates if the user has recently activated a Registrar to add an Enrollee.
   v_U16_t  DevicePasswordID;  // Device Password ID
   v_U16_t  SelectedRegistraCfgMethod; // Selected Registrar config method
   v_U8_t   UUID_E[16];        // Unique identifier of the AP.
   v_U8_t   RFBand;           // RF bands available on the AP
} tSirWPSBeaconIE;

#define SIR_WPS_ASSOCRSP_VER_PRESENT    0x00000001
#define SIR_WPS_ASSOCRSP_RESPONSETYPE_PRESENT    0x00000002

typedef struct sSirWPSAssocRspIE {
   v_U32_t FieldPresent;
   v_U32_t Version;
   v_U8_t ResposeType;
} tSirWPSAssocRspIE;

typedef struct sSirAPWPSIEs {
   tSirWPSProbeRspIE  SirWPSProbeRspIE;    /*WPS Set Probe Respose IE*/
   tSirWPSBeaconIE    SirWPSBeaconIE;      /*WPS Set Beacon IE*/
   tSirWPSAssocRspIE  SirWPSAssocRspIE;    /*WPS Set Assoc Response IE*/
} tSirAPWPSIEs, *tpSiriAPWPSIEs;

typedef struct sSirUpdateAPWPSIEsReq
{
    tANI_U16       messageType;     // eWNI_SME_UPDATE_APWPSIE_REQ
    tANI_U16       length;
    tANI_U16       transactionId;   //Transaction ID for cmd
    tSirMacAddr    bssId;           // BSSID
    tANI_U8        sessionId;       //Session ID
    tSirAPWPSIEs   APWPSIEs;
} tSirUpdateAPWPSIEsReq, *tpSirUpdateAPWPSIEsReq;

typedef struct sSirUpdateParams
{
    tANI_U16       messageType;     
    tANI_U16       length;
    tANI_U8        sessionId;      // Session ID
    tANI_U8        ssidHidden;     // Hide SSID
} tSirUpdateParams, *tpSirUpdateParams;

//Beacon Interval
typedef struct sSirChangeBIParams
{
    tANI_U16       messageType;     
    tANI_U16       length;
    tANI_U16       beaconInterval; // Beacon Interval
    tSirMacAddr    bssId;
    tANI_U8        sessionId;      // Session ID
} tSirChangeBIParams, *tpSirChangeBIParams;

typedef struct sSirOBSSHT40Param
{
   tANI_U16 OBSSScanPassiveDwellTime;
   tANI_U16 OBSSScanActiveDwellTime;
   tANI_U16 BSSChannelWidthTriggerScanInterval;
   tANI_U16 OBSSScanPassiveTotalPerChannel;
   tANI_U16 OBSSScanActiveTotalPerChannel;
   tANI_U16 BSSWidthChannelTransitionDelayFactor;
   tANI_U16 OBSSScanActivityThreshold;
}tSirOBSSHT40Param, *tpOBSSHT40Param;

#define SIR_WPS_UUID_LEN 16
#define SIR_WPS_PBC_WALK_TIME   120  // 120 Second

typedef struct sSirWPSPBCSession {
    struct sSirWPSPBCSession *next;
    tSirMacAddr              addr;
    tANI_U8                  uuid_e[SIR_WPS_UUID_LEN];
    tANI_TIMESTAMP           timestamp;
} tSirWPSPBCSession;

typedef struct sSirSmeGetWPSPBCSessionsReq
{
    tANI_U16        messageType;    // eWNI_SME_GET_WPSPBC_SESSION_REQ
    tANI_U16        length;
    void            *pUsrContext;
    void            *pSapEventCallback;
    tSirMacAddr     bssId;          // BSSID
    tSirMacAddr     pRemoveMac;      // MAC Address of STA in WPS Session to be removed
}  tSirSmeGetWPSPBCSessionsReq, *tpSirSmeGetWPSPBCSessionsReq;

typedef struct sSirWPSPBCProbeReq
{
    tSirMacAddr        peerMacAddr;
    tANI_U16           probeReqIELen;
    tANI_U8            probeReqIE[512];
} tSirWPSPBCProbeReq, *tpSirWPSPBCProbeReq;

// probereq from peer, when wsc is enabled
typedef struct sSirSmeProbeReqInd
{
    tANI_U16           messageType; //  eWNI_SME_WPS_PBC_PROBE_REQ_IND
    tANI_U16           length;
    tANI_U8            sessionId;
    tSirMacAddr        bssId;
    tSirWPSPBCProbeReq WPSPBCProbeReq;
} tSirSmeProbeReqInd, *tpSirSmeProbeReqInd;

typedef struct sSirUpdateAPWPARSNIEsReq
{
    tANI_U16       messageType;      // eWNI_SME_SET_APWPARSNIEs_REQ
    tANI_U16       length;    
    tANI_U16       transactionId; //Transaction ID for cmd
    tSirMacAddr    bssId;      // BSSID
    tANI_U8        sessionId;  //Session ID    
    tSirRSNie      APWPARSNIEs;
} tSirUpdateAPWPARSNIEsReq, *tpSirUpdateAPWPARSNIEsReq;

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
#define SIR_ROAM_MAX_CHANNELS            80
#define SIR_ROAM_SCAN_MAX_PB_REQ_SIZE    450
#define CHANNEL_LIST_STATIC                   1 /* Occupied channel list remains static */
#define CHANNEL_LIST_DYNAMIC_INIT             2 /* Occupied channel list can be learnt after init */
#define CHANNEL_LIST_DYNAMIC_FLUSH            3 /* Occupied channel list can be learnt after flush */
#define CHANNEL_LIST_DYNAMIC_UPDATE           4 /* Occupied channel list can be learnt after update */
#define SIR_ROAM_SCAN_24G_DEFAULT_CH     1
#define SIR_ROAM_SCAN_5G_DEFAULT_CH      36

/*Adaptive Thresholds to be used for FW based scanning*/
#define LFR_SENSITIVITY_THR_1MBPS             -89
#define LFR_LOOKUP_THR_1MBPS                  -78
#define LFR_SENSITIVITY_THR_2MBPS             -87
#define LFR_LOOKUP_THR_2MBPS                  -78
#define LFR_SENSITIVITY_THR_5_5MBPS           -86
#define LFR_LOOKUP_THR_5_5MBPS                -77
#define LFR_SENSITIVITY_THR_11MBPS            -85
#define LFR_LOOKUP_THR_11MBPS                 -76
#define LFR_SENSITIVITY_THR_6MBPS_2G          -83
#define LFR_LOOKUP_THR_6MBPS_2G               -78
#define LFR_SENSITIVITY_THR_6MBPS_5G          -83
#define LFR_LOOKUP_THR_6MBPS_5G               -78
#define LFR_SENSITIVITY_THR_12MBPS_2G         -83
#define LFR_LOOKUP_THR_12MBPS_2G              -78
#define LFR_SENSITIVITY_THR_12MBPS_5G         -81
#define LFR_LOOKUP_THR_12MBPS_5G              -76
#define LFR_SENSITIVITY_THR_24MBPS_2G         -81
#define LFR_LOOKUP_THR_24MBPS_2G              -76
#define LFR_SENSITIVITY_THR_24MBPS_5G         -79
#define LFR_LOOKUP_THR_24MBPS_5G              -74
#define LFR_SENSITIVITY_THR_DEFAULT             0
#define LFR_LOOKUP_THR_DEFAULT                -78
#endif //WLAN_FEATURE_ROAM_SCAN_OFFLOAD

// SME -> HAL - This is the host offload request. 
#define SIR_IPV4_ARP_REPLY_OFFLOAD                  0
#define SIR_IPV6_NEIGHBOR_DISCOVERY_OFFLOAD         1
#define SIR_IPV6_NS_OFFLOAD                         2
#define SIR_OFFLOAD_DISABLE                         0
#define SIR_OFFLOAD_ENABLE                          1
#define SIR_OFFLOAD_BCAST_FILTER_ENABLE             0x2
#define SIR_OFFLOAD_MCAST_FILTER_ENABLE             0x4
#define SIR_OFFLOAD_ARP_AND_BCAST_FILTER_ENABLE     (SIR_OFFLOAD_ENABLE|SIR_OFFLOAD_BCAST_FILTER_ENABLE)
#define SIR_OFFLOAD_NS_AND_MCAST_FILTER_ENABLE      (SIR_OFFLOAD_ENABLE|SIR_OFFLOAD_MCAST_FILTER_ENABLE)

#ifdef WLAN_NS_OFFLOAD
typedef struct sSirNsOffloadReq
{
    tANI_U8 srcIPv6Addr[16];
    tANI_U8 selfIPv6Addr[16];
    //Only support 2 possible Network Advertisement IPv6 address
    tANI_U8 targetIPv6Addr[SIR_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA][16];
    tANI_U8 selfMacAddr[6];
    tANI_U8 srcIPv6AddrValid;
    tANI_U8 targetIPv6AddrValid[SIR_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA];
    tANI_U8 slotIdx;
} tSirNsOffloadReq, *tpSirNsOffloadReq;
#endif //WLAN_NS_OFFLOAD

typedef struct sSirHostOffloadReq
{
    tANI_U8 offloadType;
    tANI_U8 enableOrDisable;
    union
    {
        tANI_U8 hostIpv4Addr [4];
        tANI_U8 hostIpv6Addr [16];
    } params;
#ifdef WLAN_NS_OFFLOAD
    tSirNsOffloadReq nsOffloadInfo;
#endif //WLAN_NS_OFFLOAD
    tSirMacAddr  bssId;
} tSirHostOffloadReq, *tpSirHostOffloadReq;

/* Packet Types. */
#define SIR_KEEP_ALIVE_NULL_PKT              1
#define SIR_KEEP_ALIVE_UNSOLICIT_ARP_RSP     2

/* Enable or disable offload. */
#define SIR_KEEP_ALIVE_DISABLE   0
#define SIR_KEEP_ALIVE_ENABLE    1

/* Keep Alive request. */
typedef struct sSirKeepAliveReq
{
    v_U8_t          packetType;
    v_U32_t         timePeriod;
    tSirIpv4Addr    hostIpv4Addr; 
    tSirIpv4Addr    destIpv4Addr;
    tSirMacAddr     destMacAddr;
    tSirMacAddr     bssId;
} tSirKeepAliveReq, *tpSirKeepAliveReq;

typedef struct sSirSmeAddStaSelfReq
{
    tANI_U16        mesgType;
    tANI_U16        mesgLen;
    tSirMacAddr     selfMacAddr;
    tVOS_CON_MODE   currDeviceMode;
}tSirSmeAddStaSelfReq, *tpSirSmeAddStaSelfReq;

typedef struct sSirSmeDelStaSelfReq
{
    tANI_U16        mesgType;
    tANI_U16        mesgLen;
    tSirMacAddr     selfMacAddr;
}tSirSmeDelStaSelfReq, *tpSirSmeDelStaSelfReq;

typedef struct sSirSmeAddStaSelfRsp
{
    tANI_U16        mesgType;
    tANI_U16        mesgLen;
    tANI_U16        status;
    tSirMacAddr     selfMacAddr;
}tSirSmeAddStaSelfRsp, *tpSirSmeAddStaSelfRsp;

typedef struct sSirSmeDelStaSelfRsp
{
    tANI_U16        mesgType;
    tANI_U16        mesgLen;
    tANI_U16        status;
    tSirMacAddr     selfMacAddr;
}tSirSmeDelStaSelfRsp, *tpSirSmeDelStaSelfRsp;

/* Coex Indication defines - 
   should match WLAN_COEX_IND_DATA_SIZE 
   should match WLAN_COEX_IND_TYPE_DISABLE_HB_MONITOR 
   should match WLAN_COEX_IND_TYPE_ENABLE_HB_MONITOR */
#define SIR_COEX_IND_DATA_SIZE (4)
#define SIR_COEX_IND_TYPE_DISABLE_HB_MONITOR (0)
#define SIR_COEX_IND_TYPE_ENABLE_HB_MONITOR (1)
#define SIR_COEX_IND_TYPE_SCAN_COMPROMISED (2)
#define SIR_COEX_IND_TYPE_SCAN_NOT_COMPROMISED (3)
#define SIR_COEX_IND_TYPE_DISABLE_AGGREGATION_IN_2p4 (4)
#define SIR_COEX_IND_TYPE_ENABLE_AGGREGATION_IN_2p4 (5)
#define SIR_COEX_IND_TYPE_ENABLE_UAPSD (6)
#define SIR_COEX_IND_TYPE_DISABLE_UAPSD (7)
#define SIR_COEX_IND_TYPE_CXM_FEATURES_NOTIFICATION (8)

typedef struct sSirSmeCoexInd
{
    tANI_U16        mesgType;
    tANI_U16        mesgLen;
    tANI_U32        coexIndType;
    tANI_U32        coexIndData[SIR_COEX_IND_DATA_SIZE];
}tSirSmeCoexInd, *tpSirSmeCoexInd;

typedef struct sSirSmeMgmtFrameInd
{
    tANI_U16        mesgType;
    tANI_U16        mesgLen;
    tANI_U32        rxChan;
    tANI_U8        sessionId;
    tANI_U8         frameType;
    tANI_S8         rxRssi;
    tANI_U8  frameBuf[1]; //variable
}tSirSmeMgmtFrameInd, *tpSirSmeMgmtFrameInd;

#ifdef WLAN_FEATURE_11W
typedef struct sSirSmeUnprotMgmtFrameInd
{
    tANI_U8         sessionId;
    tANI_U8         frameType;
    tANI_U8         frameLen;
    tANI_U8         frameBuf[1]; //variable
}tSirSmeUnprotMgmtFrameInd, *tpSirSmeUnprotMgmtFrameInd;
#endif

#define SIR_IS_FULL_POWER_REASON_DISCONNECTED(eReason) \
    ( ( eSME_LINK_DISCONNECTED_BY_HDD == (eReason) ) || \
      ( eSME_LINK_DISCONNECTED_BY_OTHER == (eReason) ) || \
      (eSME_FULL_PWR_NEEDED_BY_CHANNEL_SWITCH == (eReason)))
#define SIR_IS_FULL_POWER_NEEDED_BY_HDD(eReason) \
    ( ( eSME_LINK_DISCONNECTED_BY_HDD == (eReason) ) || ( eSME_FULL_PWR_NEEDED_BY_HDD == (eReason) ) )

/* P2P Power Save Related */
typedef struct sSirNoAParam
{
    tANI_U8 ctWindow:7;
    tANI_U8 OppPS:1;
    tANI_U8 count;
    tANI_U32 duration;
    tANI_U32 interval;
    tANI_U32 singleNoADuration;
    tANI_U8   psSelection;
}tSirNoAParam, *tpSirNoAParam;

typedef struct sSirWlanSuspendParam
{
    tANI_U8 configuredMcstBcstFilterSetting;
}tSirWlanSuspendParam,*tpSirWlanSuspendParam;

typedef struct sSirWlanResumeParam
{
    tANI_U8 configuredMcstBcstFilterSetting;
}tSirWlanResumeParam,*tpSirWlanResumeParam;

typedef struct sSirWlanSetRxpFilters
{
    tANI_U8 configuredMcstBcstFilterSetting;
    tANI_U8 setMcstBcstFilter;
}tSirWlanSetRxpFilters,*tpSirWlanSetRxpFilters;


#ifdef FEATURE_WLAN_SCAN_PNO
//
// PNO Messages
//

// Set PNO 
#define SIR_PNO_MAX_NETW_CHANNELS  26
#define SIR_PNO_MAX_NETW_CHANNELS_EX  60
#define SIR_PNO_MAX_SUPP_NETWORKS  16
#define SIR_PNO_MAX_SCAN_TIMERS    10

/*size based of dot11 declaration without extra IEs as we will not carry those for PNO*/
#define SIR_PNO_MAX_PB_REQ_SIZE    450 

#define SIR_PNO_24G_DEFAULT_CH     1
#define SIR_PNO_5G_DEFAULT_CH      36

typedef enum
{
   SIR_PNO_MODE_IMMEDIATE,
   SIR_PNO_MODE_ON_SUSPEND,
   SIR_PNO_MODE_ON_RESUME,
   SIR_PNO_MODE_MAX 
} eSirPNOMode;

typedef struct 
{
  tSirMacSSid ssId;
  tANI_U32    authentication; 
  tANI_U32    encryption; 
  tANI_U32    bcastNetwType; 
  tANI_U8     ucChannelCount;
  tANI_U8     aChannels[SIR_PNO_MAX_NETW_CHANNELS_EX];
  tANI_U8     rssiThreshold;
} tSirNetworkType; 

typedef struct 
{
  tANI_U32    uTimerValue; 
  tANI_U32    uTimerRepeat; 
}tSirScanTimer; 

typedef struct
{
  tANI_U8        ucScanTimersCount; 
  tSirScanTimer  aTimerValues[SIR_PNO_MAX_SCAN_TIMERS]; 
} tSirScanTimersType;

/*Pref Net Req status */
typedef void(*PNOReqStatusCb)(void *callbackContext, VOS_STATUS status);


typedef struct sSirPNOScanReq
{
  tANI_U8             enable;
  PNOReqStatusCb      statusCallback;
  void                *callbackContext;
  eSirPNOMode         modePNO;
  tANI_U8             ucNetworksCount; 
  tSirNetworkType     aNetworks[SIR_PNO_MAX_SUPP_NETWORKS];
  tSirScanTimersType  scanTimers;
  
  /*added by SME*/
  tANI_U16  us24GProbeTemplateLen; 
  tANI_U8   p24GProbeTemplate[SIR_PNO_MAX_PB_REQ_SIZE];
  tANI_U16  us5GProbeTemplateLen; 
  tANI_U8   p5GProbeTemplate[SIR_PNO_MAX_PB_REQ_SIZE]; 
} tSirPNOScanReq, *tpSirPNOScanReq;

typedef struct sSirSetRSSIFilterReq
{
  tANI_U8     rssiThreshold;
} tSirSetRSSIFilterReq, *tpSirSetRSSIFilterReq;


// Update Scan Params
typedef struct {
  tANI_U8   b11dEnabled;
  tANI_U8   b11dResolved;
  tANI_U8   ucChannelCount;
  tANI_U8   aChannels[SIR_PNO_MAX_NETW_CHANNELS_EX];
  tANI_U16  usPassiveMinChTime;
  tANI_U16  usPassiveMaxChTime;
  tANI_U16  usActiveMinChTime;
  tANI_U16  usActiveMaxChTime;
  tANI_U8   ucCBState;
} tSirUpdateScanParams, * tpSirUpdateScanParams;

// Preferred Network Found Indication
typedef struct
{
  tANI_U16      mesgType;
  tANI_U16      mesgLen;
  /* Network that was found with the highest RSSI*/
  tSirMacSSid   ssId;
  /* Indicates the RSSI */
  tANI_U8       rssi;
  /* Length of the beacon or probe response
   * corresponding to the candidate found by PNO */
  tANI_U32      frameLength;
  /* Index to memory location where the contents of
   * beacon or probe response frame will be copied */
  tANI_U8       data[1];
} tSirPrefNetworkFoundInd, *tpSirPrefNetworkFoundInd;
#endif //FEATURE_WLAN_SCAN_PNO

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
typedef struct
{
  tSirMacSSid ssId;
  tANI_U8     currAPbssid[WNI_CFG_BSSID_LEN];
  tANI_U32    authentication;
  tANI_U8     encryption;
  tANI_U8     mcencryption;
  tANI_U8     ChannelCount;
  tANI_U8     ChannelCache[SIR_ROAM_MAX_CHANNELS];

} tSirRoamNetworkType;

typedef struct SirMobilityDomainInfo
{
  tANI_U8 mdiePresent;
  tANI_U16 mobilityDomain;
} tSirMobilityDomainInfo;

typedef struct sSirRoamOffloadScanReq
{
  eAniBoolean RoamScanOffloadEnabled;
  eAniBoolean MAWCEnabled;
  tANI_S8     LookupThreshold;
  tANI_S8     RxSensitivityThreshold;
  tANI_U8     RoamRssiDiff;
  tANI_U8     ChannelCacheType;
  tANI_U8     Command;
  tANI_U8     StartScanReason;
  tANI_U16    NeighborScanTimerPeriod;
  tANI_U16    NeighborRoamScanRefreshPeriod;
  tANI_U16    NeighborScanChannelMinTime;
  tANI_U16    NeighborScanChannelMaxTime;
  tANI_U16    EmptyRefreshScanPeriod;
  tANI_U8     ValidChannelCount;
  tANI_U8     ValidChannelList[SIR_ROAM_MAX_CHANNELS];
  eAniBoolean IsESEEnabled;
  tANI_U16  us24GProbeTemplateLen;
  tANI_U8   p24GProbeTemplate[SIR_ROAM_SCAN_MAX_PB_REQ_SIZE];
  tANI_U16  us5GProbeTemplateLen;
  tANI_U8   p5GProbeTemplate[SIR_ROAM_SCAN_MAX_PB_REQ_SIZE];
  tANI_U8   nProbes;
  tANI_U16  HomeAwayTime;
  tSirRoamNetworkType ConnectedNetwork;
  tSirMobilityDomainInfo MDID;
} tSirRoamOffloadScanReq, *tpSirRoamOffloadScanReq;
#endif //WLAN_FEATURE_ROAM_SCAN_OFFLOAD

#define SIR_NOCHANGE_POWER_VALUE  0xFFFFFFFF

//Power Parameters Type
typedef enum
{
   eSIR_IGNORE_DTIM        = 1,
   eSIR_LISTEN_INTERVAL    = 2, 
   eSIR_MCAST_BCAST_FILTER = 3, 
   eSIR_ENABLE_BET         = 4, 
   eSIR_BET_INTERVAL       = 5 
}tPowerParamType;

//Power Parameters Value s
typedef struct 
{
  /*  Ignore DTIM */
  tANI_U32 uIgnoreDTIM;

  /* DTIM Period */
  tANI_U32 uDTIMPeriod; 

  /* Listen Interval */
  tANI_U32 uListenInterval;

  /* Broadcast Multicas Filter  */
  tANI_U32 uBcastMcastFilter;

  /* Beacon Early Termination */
  tANI_U32 uEnableBET;

  /* Beacon Early Termination Interval */
  tANI_U32 uBETInterval; 

  /* MAX LI for modulated DTIM */
  tANI_U32 uMaxLIModulatedDTIM;

}tSirSetPowerParamsReq, *tpSirSetPowerParamsReq;

typedef struct sSirTxPerTrackingParam
{
    tANI_U8  ucTxPerTrackingEnable;           /* 0: disable, 1:enable */
    tANI_U8  ucTxPerTrackingPeriod;              /* Check period, unit is sec. Once tx_stat_chk enable, firmware will check PER in this period periodically */
    tANI_U8  ucTxPerTrackingRatio;            /* (Fail TX packet)/(Total TX packet) ratio, the unit is 10%. for example, 5 means 50% TX failed rate, default is 5. If current TX packet failed rate bigger than this ratio then firmware send WLC_E_TX_STAT_ERROR event to driver */
    tANI_U32 uTxPerTrackingWatermark;               /* A watermark of check number, once the tx packet exceed this number, we do the check, default is 5 */
}tSirTxPerTrackingParam, *tpSirTxPerTrackingParam;

#ifdef WLAN_FEATURE_PACKET_FILTERING
/*---------------------------------------------------------------------------
  Packet Filtering Parameters
---------------------------------------------------------------------------*/
#define    SIR_IPV4_ADDR_LEN                 4
#define    SIR_MAC_ADDR_LEN                  6
#define    SIR_MAX_FILTER_TEST_DATA_LEN       8
#define    SIR_MAX_NUM_MULTICAST_ADDRESS    240
#define    SIR_MAX_NUM_FILTERS               20 
#define    SIR_MAX_NUM_TESTS_PER_FILTER      10 

//
// Receive Filter Parameters
//
typedef enum
{
  SIR_RCV_FILTER_TYPE_INVALID,
  SIR_RCV_FILTER_TYPE_FILTER_PKT,
  SIR_RCV_FILTER_TYPE_BUFFER_PKT,
  SIR_RCV_FILTER_TYPE_MAX_ENUM_SIZE
}eSirReceivePacketFilterType;

typedef enum 
{
  SIR_FILTER_HDR_TYPE_INVALID,
  SIR_FILTER_HDR_TYPE_MAC,
  SIR_FILTER_HDR_TYPE_ARP,
  SIR_FILTER_HDR_TYPE_IPV4,
  SIR_FILTER_HDR_TYPE_IPV6,
  SIR_FILTER_HDR_TYPE_UDP,
  SIR_FILTER_HDR_TYPE_MAX
}eSirRcvPktFltProtocolType;

typedef enum 
{
  SIR_FILTER_CMP_TYPE_INVALID,
  SIR_FILTER_CMP_TYPE_EQUAL,
  SIR_FILTER_CMP_TYPE_MASK_EQUAL,
  SIR_FILTER_CMP_TYPE_NOT_EQUAL,
  SIR_FILTER_CMP_TYPE_MASK_NOT_EQUAL,
  SIR_FILTER_CMP_TYPE_MAX
}eSirRcvPktFltCmpFlagType;

typedef struct sSirRcvPktFilterFieldParams
{
  eSirRcvPktFltProtocolType        protocolLayer;
  eSirRcvPktFltCmpFlagType         cmpFlag;
  /* Length of the data to compare */
  tANI_U16                         dataLength; 
  /* from start of the respective frame header */
  tANI_U8                          dataOffset; 
  /* Reserved field */
  tANI_U8                          reserved; 
  /* Data to compare */
  tANI_U8                          compareData[SIR_MAX_FILTER_TEST_DATA_LEN];
  /* Mask to be applied on the received packet data before compare */
  tANI_U8                          dataMask[SIR_MAX_FILTER_TEST_DATA_LEN];   
}tSirRcvPktFilterFieldParams, *tpSirRcvPktFilterFieldParams;

typedef struct sSirRcvPktFilterCfg
{
  tANI_U8                         filterId; 
  eSirReceivePacketFilterType     filterType;
  tANI_U32                        numFieldParams;
  tANI_U32                        coalesceTime;
  tSirMacAddr                     selfMacAddr;
  tSirMacAddr                     bssId; //Bssid of the connected AP
  tSirRcvPktFilterFieldParams     paramsData[SIR_MAX_NUM_TESTS_PER_FILTER];
}tSirRcvPktFilterCfgType, *tpSirRcvPktFilterCfgType;

//
// Filter Packet Match Count Parameters
//
typedef struct sSirRcvFltPktMatchCnt
{
  tANI_U8    filterId;
  tANI_U32   matchCnt;
} tSirRcvFltPktMatchCnt, tpSirRcvFltPktMatchCnt;

typedef struct sSirRcvFltPktMatchRsp
{
  tANI_U16        mesgType;
  tANI_U16        mesgLen;
    
  /* Success or Failure */
  tANI_U32                 status;
  tSirRcvFltPktMatchCnt    filterMatchCnt[SIR_MAX_NUM_FILTERS];
  tSirMacAddr      bssId;
} tSirRcvFltPktMatchRsp, *tpSirRcvFltPktMatchRsp;

//
// Receive Filter Clear Parameters
//
typedef struct sSirRcvFltPktClearParam
{
  tANI_U32   status;  /* only valid for response message */
  tANI_U8    filterId;
  tSirMacAddr selfMacAddr;
  tSirMacAddr bssId;
}tSirRcvFltPktClearParam, *tpSirRcvFltPktClearParam;

//
// Multicast Address List Parameters
//
typedef struct sSirRcvFltMcAddrList
{
  tANI_U32       ulMulticastAddrCnt;
  tSirMacAddr    multicastAddr[SIR_MAX_NUM_MULTICAST_ADDRESS];
  tSirMacAddr    selfMacAddr;
  tSirMacAddr    bssId;
} tSirRcvFltMcAddrList, *tpSirRcvFltMcAddrList;
#endif // WLAN_FEATURE_PACKET_FILTERING

//
// Generic version information
//
typedef struct
{
  tANI_U8    revision;
  tANI_U8    version;
  tANI_U8    minor;
  tANI_U8    major;
} tSirVersionType;

typedef struct sAniBtAmpLogLinkReq
{
    // Common for all types are requests
    tANI_U16                msgType;    // message type is same as the request type
    tANI_U16                msgLen;  // length of the entire request
    tANI_U8                 sessionId; //sme Session Id
    void                   *btampHandle; //AMP context
    
} tAniBtAmpLogLinkReq, *tpAniBtAmpLogLinkReq;

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/*---------------------------------------------------------------------------
* WDA_GTK_OFFLOAD_REQ
*--------------------------------------------------------------------------*/
typedef struct
{
  tANI_U32     ulFlags;             /* optional flags */
  tANI_U8      aKCK[16];            /* Key confirmation key */ 
  tANI_U8      aKEK[16];            /* key encryption key */
  tANI_U64     ullKeyReplayCounter; /* replay counter */
  tSirMacAddr  bssId;
} tSirGtkOffloadParams, *tpSirGtkOffloadParams;

/*---------------------------------------------------------------------------
* WDA_GTK_OFFLOAD_GETINFO_REQ
*--------------------------------------------------------------------------*/
typedef struct
{
   tANI_U16   mesgType;
   tANI_U16   mesgLen;

   tANI_U32   ulStatus;             /* success or failure */
   tANI_U64   ullKeyReplayCounter;  /* current replay counter value */
   tANI_U32   ulTotalRekeyCount;    /* total rekey attempts */
   tANI_U32   ulGTKRekeyCount;      /* successful GTK rekeys */
   tANI_U32   ulIGTKRekeyCount;     /* successful iGTK rekeys */
   tSirMacAddr bssId;
} tSirGtkOffloadGetInfoRspParams, *tpSirGtkOffloadGetInfoRspParams;
#endif // WLAN_FEATURE_GTK_OFFLOAD

#ifdef WLAN_WAKEUP_EVENTS
/*---------------------------------------------------------------------------
  tSirWakeReasonInd    
---------------------------------------------------------------------------*/
typedef struct
{  
    tANI_U16      mesgType;
    tANI_U16      mesgLen;
    tANI_U32      ulReason;        /* see tWakeReasonType */
    tANI_U32      ulReasonArg;     /* argument specific to the reason type */
    tANI_U32      ulStoredDataLen; /* length of optional data stored in this message, in case
                              HAL truncates the data (i.e. data packets) this length
                              will be less than the actual length */
    tANI_U32      ulActualDataLen; /* actual length of data */
    tANI_U8       aDataStart[1];  /* variable length start of data (length == storedDataLen)
                             see specific wake type */ 
} tSirWakeReasonInd, *tpSirWakeReasonInd;
#endif // WLAN_WAKEUP_EVENTS

/*---------------------------------------------------------------------------
  sAniSetTmLevelReq    
---------------------------------------------------------------------------*/
typedef struct sAniSetTmLevelReq
{
    tANI_U16                tmMode;
    tANI_U16                newTmLevel;
} tAniSetTmLevelReq, *tpAniSetTmLevelReq;

#ifdef FEATURE_WLAN_TDLS
/* TDLS Request struct SME-->PE */
typedef struct sSirTdlsSendMgmtReq
{
    tANI_U16            messageType;   // eWNI_SME_TDLS_DISCOVERY_START_REQ
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tANI_U8             reqType;
    tANI_U8             dialog;
    tANI_U16            statusCode;
    tANI_U8             responder;
    tANI_U32            peerCapability;
    tSirMacAddr         bssid;         // For multi-session, for PE to locate peSession ID
    tSirMacAddr         peerMac;
    tANI_U8             addIe[1];      //Variable lenght. Dont add any field after this.
} tSirTdlsSendMgmtReq, *tpSirSmeTdlsSendMgmtReq ;

typedef enum TdlsAddOper
{
    TDLS_OPER_NONE,
    TDLS_OPER_ADD,
    TDLS_OPER_UPDATE
} eTdlsAddOper;

/* TDLS Request struct SME-->PE */
typedef struct sSirTdlsAddStaReq
{
    tANI_U16            messageType;   // eWNI_SME_TDLS_DISCOVERY_START_REQ
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tSirMacAddr         bssid;         // For multi-session, for PE to locate peSession ID
    eTdlsAddOper        tdlsAddOper;
    tSirMacAddr         peerMac;
    tANI_U16            capability;
    tANI_U8             extn_capability[SIR_MAC_MAX_EXTN_CAP];
    tANI_U8             supported_rates_length;
    tANI_U8             supported_rates[SIR_MAC_MAX_SUPP_RATES];
    tANI_U8             htcap_present;
    tSirHTCap           htCap;
    tANI_U8             vhtcap_present;
    tSirVHTCap          vhtCap;
    tANI_U8             uapsd_queues;
    tANI_U8             max_sp;
} tSirTdlsAddStaReq, *tpSirSmeTdlsAddStaReq ;

/* TDLS Response struct PE-->SME */
typedef struct sSirTdlsAddStaRsp
{
    tANI_U16               messageType;
    tANI_U16               length;
    tSirResultCodes        statusCode;
    tSirMacAddr            peerMac;
    tANI_U8                sessionId;     // Session ID
    tANI_U16               staId ;
    tANI_U16               staType ;
    tANI_U8                ucastSig;
    tANI_U8                bcastSig;
    eTdlsAddOper           tdlsAddOper;
} tSirTdlsAddStaRsp ;

/* TDLS Request struct SME-->PE */
typedef struct
{
    tANI_U16            messageType;   // eWNI_SME_TDLS_LINK_ESTABLISH_REQ
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tANI_U8             uapsdQueues;   // Peer's uapsd Queues Information
    tANI_U8             maxSp;         // Peer's Supported Maximum Service Period
    tANI_U8             isBufSta;      // Does Peer Support as Buffer Station.
    tANI_U8             isOffChannelSupported;    // Does Peer Support as TDLS Off Channel.
    tANI_U8             isResponder;   // Is Peer a responder.
    tSirMacAddr         bssid;         // For multi-session, for PE to locate peSession ID
    tSirMacAddr         peerMac;
    tANI_U8             supportedChannelsLen;
    tANI_U8             supportedChannels[SIR_MAC_MAX_SUPP_CHANNELS];
    tANI_U8             supportedOperClassesLen;
    tANI_U8             supportedOperClasses[SIR_MAC_MAX_SUPP_OPER_CLASSES];
}tSirTdlsLinkEstablishReq, *tpSirTdlsLinkEstablishReq;

/* TDLS Request struct SME-->PE */
typedef struct
{
    tANI_U16            messageType;   // eWNI_SME_TDLS_LINK_ESTABLISH_RSP
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tSirResultCodes        statusCode;
    tSirMacAddr            peerMac;
}tSirTdlsLinkEstablishReqRsp, *tpSirTdlsLinkEstablishReqRsp;

/* TDLS Request struct SME-->PE */
typedef struct sSirTdlsDelStaReq
{
    tANI_U16            messageType;   // eWNI_SME_TDLS_DISCOVERY_START_REQ
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tSirMacAddr         bssid;         // For multi-session, for PE to locate peSession ID
    tSirMacAddr         peerMac;
} tSirTdlsDelStaReq, *tpSirSmeTdlsDelStaReq ;
/* TDLS Response struct PE-->SME */
typedef struct sSirTdlsDelStaRsp
{
   tANI_U16               messageType;
   tANI_U16               length;
   tANI_U8                sessionId;     // Session ID
   tSirResultCodes        statusCode;
   tSirMacAddr            peerMac;
   tANI_U16               staId;
} tSirTdlsDelStaRsp, *tpSirTdlsDelStaRsp;
/* TDLS Delete Indication struct PE-->SME */
typedef struct sSirTdlsDelStaInd
{
   tANI_U16               messageType;
   tANI_U16               length;
   tANI_U8                sessionId;     // Session ID
   tSirMacAddr            peerMac;
   tANI_U16               staId;
   tANI_U16               reasonCode;
} tSirTdlsDelStaInd, *tpSirTdlsDelStaInd;
typedef struct sSirTdlsDelAllPeerInd
{
   tANI_U16               messageType;
   tANI_U16               length;
   tANI_U8                sessionId;     // Session ID
} tSirTdlsDelAllPeerInd, *tpSirTdlsDelAllPeerInd;
typedef struct sSirMgmtTxCompletionInd
{
   tANI_U16               messageType;
   tANI_U16               length;
   tANI_U8                sessionId;     // Session ID
   tANI_U32               txCompleteStatus;
} tSirMgmtTxCompletionInd, *tpSirMgmtTxCompletionInd;

//tdlsoffchan
/* TDLS Channel Switch struct SME-->PE */
typedef struct
{
    tANI_U16            messageType;  //eWNI_SME_TDLS_CHANNEL_SWITCH_REQ
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tANI_U8             tdlsOffCh;     // Target Off Channel
    tANI_U8             tdlsOffChBwOffset;// Target Off Channel Bandwidth offset
    tANI_U8             tdlsSwMode;     // TDLS Off Channel Mode
    tSirMacAddr         bssid;         // For multi-session, for PE to locate peSession ID
    tSirMacAddr         peerMac;
}tSirTdlsChanSwitch, *tpSirTdlsChanSwitch;

/* TDLS Resp struct */
typedef struct
{
    tANI_U16            messageType;   // eWNI_SME_TDLS_CHANNEL_SWITCH_RSP
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tSirResultCodes        statusCode;
    tSirMacAddr            peerMac;
}tSirTdlsChanSwitchReqRsp, *tpSirTdlsChanSwitchReqRsp;
#endif /* FEATURE_WLAN_TDLS */

#ifdef FEATURE_WLAN_TDLS_INTERNAL
typedef enum tdlsListType
{
    TDLS_DIS_LIST,
    TDLS_SETUP_LIST
}eTdlsListType ;

typedef enum tdlsStates
{
    TDLS_LINK_IDLE_STATE,
    TDLS_LINK_DIS_INIT_STATE,
    TDLS_LINK_DIS_RSP_WAIT_STATE,
    TDLS_DIS_REQ_PROCESS_STATE,
    TDLS_DIS_RSP_SENT_WAIT_STATE,
    TDLS_DIS_RSP_SENT_DONE_STATE,
    TDLS_LINK_DIS_DONE_STATE,
    TDLS_LINK_SETUP_START_STATE,
    TDLS_LINK_SETUP_WAIT_STATE,
    TDLS_LINK_SETUP_RSP_WAIT_STATE,
    TDLS_LINK_SETUP_DONE_STATE,
    TDLS_LINK_TEARDOWN_START_STATE,
    TDLS_LINK_TEARDOWN_DONE_STATE,
    TDLS_LINK_SETUP_STATE
}eSirTdlsStates ;

typedef struct sSirTdlsPeerInfo
{
    tSirMacAddr peerMac;
    tANI_U8     sessionId;
    tANI_U8     dialog ;
    tSirMacCapabilityInfo capabilityInfo ;
    tSirMacRateSet  tdlsPeerSuppRates ;
    tSirMacRateSet  tdlsPeerExtRates ;
    //tDot11fIEHTCaps tdlsPeerHtCaps ;
    tSirMacHTCapabilityInfo tdlsPeerHtCaps ;
    tSirMacHTParametersInfo tdlsPeerHtParams ;
    tSirMacExtendedHTCapabilityInfo tdlsPeerHtExtCaps ;
    tANI_U8  supportedMCSSet[SIZE_OF_SUPPORTED_MCS_SET];

    //tDot11fIEExtCapability tdlsPeerExtenCaps ;
    tSirMacRsnInfo tdlsPeerRsn ;
    tANI_U16 tdlsPeerFtIe ;
    tANI_U16 tdlsPeerTimeoutIntvl ;
    tANI_U16 tdlsPeerSuppChan ;
    tANI_U16 tdlsPeerSuppReguClass ;
    tANI_S8  tdlsPeerRssi ;
    tANI_U16 tdlsPeerState ;
    /* flags to indicate optional IE's are in */
    tANI_U8  ExtRatesPresent ;
    tANI_U8  rsnIePresent ;
    tANI_U8  htCapPresent ;
    tANI_U8  delStaNeeded ;

} tSirTdlsPeerInfo, *tpSirSmeTdlsPeerInfo ;

/* TDLS Request struct SME-->PE */
typedef struct sSirTdlsDiscoveryReq
{
    tANI_U16            messageType;   // eWNI_SME_TDLS_DISCOVERY_START_REQ
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tANI_U8             reqType;
    tANI_U8             dialog;
    tSirMacAddr         bssid;         // For multi-session, for PE to locate peSession ID
    tSirMacAddr         peerMac;
} tSirTdlsDisReq, *tpSirSmeTdlsDisReq ;

typedef struct sSirTdlsLinkSetupReq
{
    tANI_U16            messageType;   // eWNI_SME_TDLS_LINK_START_REQ
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tANI_U8             dialog;
    tSirMacAddr         bssid;         // For multi-session, for PE to locate peSession ID
    tSirMacAddr         peerMac;
} tSirTdlsSetupReq, *tpSirSmeTdlsSetupReq ;

typedef struct sSirTdlsTeardownReq
{
    tANI_U16            messageType;   // eWNI_SME_TDLS_TEARDOWN_REQ
    tANI_U16            length;
    tANI_U8             sessionId;     // Session ID
    tANI_U16            transactionId; // Transaction ID for cmd
    tSirMacAddr         bssid;         // For multi-session, for PE to locate peSession ID
    tSirMacAddr         peerMac;
} tSirTdlsTeardownReq, *tpSirSmeTdlsTeardownReq ;


/* TDLS response struct  PE-->SME */
typedef struct sSirTdlsDiscoveryRsp
{
    tANI_U16               messageType;
    tANI_U16               length;
    tSirResultCodes        statusCode;
    tANI_U16               numDisSta ;
    tSirTdlsPeerInfo       tdlsDisPeerInfo[0];
} tSirTdlsDisRsp, *tpSirSmeTdlsDiscoveryRsp;

typedef struct sSirTdlsLinkSetupRsp
{
    tANI_U16               messageType;
    tANI_U16               length;
    tSirResultCodes        statusCode;
    tSirMacAddr            peerMac;
} tSirTdlsLinksetupRsp ;

typedef struct sSirTdlsLinkSetupInd
{
    tANI_U16               messageType;
    tANI_U16               length;
    tSirResultCodes        statusCode;
    tSirMacAddr            peerMac;
} tSirTdlsLinkSetupInd ;


typedef struct sSirTdlsTeardownRsp
{
    tANI_U16               messageType;
    tANI_U16               length;
    tSirResultCodes        statusCode;
    tSirMacAddr            peerMac;
} tSirTdlsTeardownRsp ;

typedef struct sSirTdlsPeerInd
{
    tANI_U16               messageType;
    tANI_U16               length;
    tSirMacAddr            peerMac;
    tANI_U8                sessionId;     // Session ID
    tANI_U16               staId ;
    tANI_U16               staType ;
    tANI_U8                ucastSig;
    tANI_U8                bcastSig;
} tSirTdlsPeerInd ;

typedef struct sSirTdlsLinkEstablishInd
{
    tANI_U16               messageType;
    tANI_U16               length;
    tANI_U8                bIsResponder;  /* if this is 1, self is initiator and peer is reponder */
    tANI_U8                linkIdenOffset;  /* offset of LinkIdentifierIE.bssid[0] from ptiTemplateBuf */
    tANI_U8                ptiBufStatusOffset; /* offset of BufferStatus from ptiTemplateBuf */
    tANI_U8                ptiTemplateLen;
    tANI_U8                ptiTemplateBuf[64];
    tANI_U8                extCapability[8];
/*  This will be part of PTI template when sent by PE  
    tANI_U8                linkIdentifier[20];
*/    
} tSirTdlsLinkEstablishInd, *tpSirTdlsLinkEstablishInd;

typedef struct sSirTdlsLinkTeardownInd
{
   tANI_U16               messageType;
   tANI_U16               length;
   tANI_U16               staId;
} tSirTdlsLinkTeardownInd, *tpSirTdlsLinkTeardownInd;

#endif  /* FEATURE_WLAN_TDLS_INTERNAL */

typedef struct sSirActiveModeSetBcnFilterReq
{
   tANI_U16               messageType;
   tANI_U16               length;
   tANI_U8                seesionId;
} tSirSetActiveModeSetBncFilterReq, *tpSirSetActiveModeSetBncFilterReq;

typedef enum
{
   HT40_OBSS_SCAN_PARAM_START,
   HT40_OBSS_SCAN_PARAM_UPDATE
}tHT40OBssScanCmdType;

typedef struct sSirSmeHT40StopOBSSScanInd
{
   tANI_U16               messageType;
   tANI_U16               length;
   tANI_U8                seesionId;
} tSirSmeHT40OBSSStopScanInd, *tpSirSmeHT40OBSSStopScanInd;

typedef struct sSirSmeHT40OBSSScanInd
{
   tANI_U16               messageType;
   tANI_U16               length;
   tSirMacAddr            peerMacAddr;
} tSirSmeHT40OBSSScanInd, *tpSirSmeHT40OBSSScanInd;

typedef struct sSirHT40OBSSScanInd
{
    tHT40OBssScanCmdType cmdType;
    tSirScanType scanType;
    tANI_U16     OBSSScanPassiveDwellTime; // In TUs
    tANI_U16     OBSSScanActiveDwellTime;  // In TUs
    tANI_U16     BSSChannelWidthTriggerScanInterval; // In seconds
    tANI_U16     OBSSScanPassiveTotalPerChannel; // In TU
    tANI_U16     OBSSScanActiveTotalPerChannel;  // In TUs
    tANI_U16     BSSWidthChannelTransitionDelayFactor;
    tANI_U16     OBSSScanActivityThreshold;
    tANI_U8      selfStaIdx;
    tANI_U8      bssIdx;
    tANI_U8      fortyMHZIntolerent;
    tANI_U8      channelCount;
    tANI_U8      channels[SIR_ROAM_MAX_CHANNELS];
    tANI_U8      currentOperatingClass;
    tANI_U16     ieFieldLen;
    tANI_U8      ieField[SIR_ROAM_SCAN_MAX_PB_REQ_SIZE];
} tSirHT40OBSSScanInd, *tpSirHT40OBSSScanInd;


//Reset AP Caps Changed
typedef struct sSirResetAPCapsChange
{
    tANI_U16       messageType;
    tANI_U16       length;
    tSirMacAddr    bssId;
} tSirResetAPCapsChange, *tpSirResetAPCapsChange;
/// Definition for Candidate found indication from FW
typedef struct sSirSmeCandidateFoundInd
{
    tANI_U16            messageType; // eWNI_SME_CANDIDATE_FOUND_IND
    tANI_U16            length;
    tANI_U8             sessionId;  // Session Identifier
} tSirSmeCandidateFoundInd, *tpSirSmeCandidateFoundInd;

#ifdef WLAN_FEATURE_11W
typedef struct sSirWlanExcludeUnencryptParam
{
    tANI_BOOLEAN    excludeUnencrypt;
    tSirMacAddr     bssId;
}tSirWlanExcludeUnencryptParam,*tpSirWlanExcludeUnencryptParam;
#endif

typedef struct sAniHandoffReq
{
    // Common for all types are requests
    tANI_U16  msgType; // message type is same as the request type
    tANI_U16  msgLen;  // length of the entire request
    tANI_U8   sessionId;
    tANI_U8   bssid[WNI_CFG_BSSID_LEN];
    tANI_U8   channel;
} tAniHandoffReq, *tpAniHandoffReq;

typedef struct sSirScanOffloadReq {
    tANI_U8 sessionId;
    tSirMacAddr bssId;
    tANI_U8 numSsid;
    tSirMacSSid ssId[SIR_SCAN_MAX_NUM_SSID];
    tANI_U8 hiddenSsid;
    tSirMacAddr selfMacAddr;
    tSirBssType bssType;
    tANI_U8 dot11mode;
    tSirScanType scanType;
    tANI_U32 minChannelTime;
    tANI_U32 maxChannelTime;
    tANI_BOOLEAN p2pSearch;
    tANI_U16 uIEFieldLen;
    tANI_U16 uIEFieldOffset;
    tSirChannelList channelList;
    /*-----------------------------
      sSirScanOffloadReq....
      -----------------------------
      uIEFieldLen
      -----------------------------
      uIEFieldOffset               ----+
      -----------------------------    |
      channelList.numChannels          |
      -----------------------------    |
      ... variable size up to          |
      channelNumber[numChannels-1]     |
      This can be zero, if             |
      numChannel is zero.              |
      ----------------------------- <--+
      ... variable size uIEField
      up to uIEFieldLen (can be 0)
      -----------------------------*/
} tSirScanOffloadReq, *tpSirScanOffloadReq;

typedef enum sSirScanEventType {
    SCAN_EVENT_STARTED=0x1,          /* Scan command accepted by FW */
    SCAN_EVENT_COMPLETED=0x2,        /* Scan has been completed by FW */
    SCAN_EVENT_BSS_CHANNEL=0x4,      /* FW is going to move to HOME channel */
    SCAN_EVENT_FOREIGN_CHANNEL = 0x8,/* FW is going to move to FORIEGN channel */
    SCAN_EVENT_DEQUEUED=0x10,       /* scan request got dequeued */
    SCAN_EVENT_PREEMPTED=0x20,      /* preempted by other high priority scan */
    SCAN_EVENT_START_FAILED=0x40,   /* scan start failed */
    SCAN_EVENT_RESTARTED=0x80,      /*scan restarted*/
    SCAN_EVENT_MAX=0x8000
} tSirScanEventType;

typedef struct sSirScanOffloadEvent{
    tSirScanEventType event;
    tSirResultCodes reasonCode;
    tANI_U32 chanFreq;
    tANI_U32 requestor;
    tANI_U32 scanId;
} tSirScanOffloadEvent, *tpSirScanOffloadEvent;

typedef struct sSirUpdateChanParam
{
    tANI_U8 chanId;
    tANI_U8 pwr;
    tANI_BOOLEAN dfsSet;
} tSirUpdateChanParam, *tpSirUpdateChanParam;

typedef struct sSirUpdateChan
{
    tANI_U8 numChan;
    tSirUpdateChanParam chanParam[1];
} tSirUpdateChanList, *tpSirUpdateChanList;

#ifdef FEATURE_WLAN_LPHB
#define SIR_LPHB_FILTER_LEN   64

typedef enum
{
   LPHB_SET_EN_PARAMS_INDID,
   LPHB_SET_TCP_PARAMS_INDID,
   LPHB_SET_TCP_PKT_FILTER_INDID,
   LPHB_SET_UDP_PARAMS_INDID,
   LPHB_SET_UDP_PKT_FILTER_INDID,
   LPHB_SET_NETWORK_INFO_INDID,
} LPHBIndType;

typedef struct sSirLPHBEnableStruct
{
   v_U8_t enable;
   v_U8_t item;
   v_U8_t session;
} tSirLPHBEnableStruct;

typedef struct sSirLPHBTcpParamStruct
{
   v_U32_t      srv_ip;
   v_U32_t      dev_ip;
   v_U16_t      src_port;
   v_U16_t      dst_port;
   v_U16_t      timeout;
   v_U8_t       session;
   tSirMacAddr  gateway_mac;
   uint16       timePeriodSec; // in seconds
   uint32       tcpSn;
} tSirLPHBTcpParamStruct;

typedef struct sSirLPHBTcpFilterStruct
{
   v_U16_t length;
   v_U8_t  offset;
   v_U8_t  session;
   v_U8_t  filter[SIR_LPHB_FILTER_LEN];
} tSirLPHBTcpFilterStruct;

typedef struct sSirLPHBUdpParamStruct
{
   v_U32_t      srv_ip;
   v_U32_t      dev_ip;
   v_U16_t      src_port;
   v_U16_t      dst_port;
   v_U16_t      interval;
   v_U16_t      timeout;
   v_U8_t       session;
   tSirMacAddr  gateway_mac;
} tSirLPHBUdpParamStruct;

typedef struct sSirLPHBUdpFilterStruct
{
   v_U16_t length;
   v_U8_t  offset;
   v_U8_t  session;
   v_U8_t  filter[SIR_LPHB_FILTER_LEN];
} tSirLPHBUdpFilterStruct;

typedef struct sSirLPHBReq
{
   v_U16_t cmd;
   v_U16_t dummy;
   union
   {
      tSirLPHBEnableStruct     lphbEnableReq;
      tSirLPHBTcpParamStruct   lphbTcpParamReq;
      tSirLPHBTcpFilterStruct  lphbTcpFilterReq;
      tSirLPHBUdpParamStruct   lphbUdpParamReq;
      tSirLPHBUdpFilterStruct  lphbUdpFilterReq;
   } params;
} tSirLPHBReq;

typedef struct sSirLPHBInd
{
   v_U8_t sessionIdx;
   v_U8_t protocolType; /*TCP or UDP*/
   v_U8_t eventReason;
} tSirLPHBInd;
#endif /* FEATURE_WLAN_LPHB */

typedef struct sSirAddPeriodicTxPtrn
{
    /* MAC Address for the adapter */
    tSirMacAddr macAddress;

    tANI_U8  ucPtrnId;           // Pattern ID
    tANI_U16 ucPtrnSize;         // Pattern size
    tANI_U32 usPtrnIntervalMs;   // In msec
    tANI_U8  ucPattern[PERIODIC_TX_PTRN_MAX_SIZE]; // Pattern buffer
} tSirAddPeriodicTxPtrn, *tpSirAddPeriodicTxPtrn;

typedef struct sSirDelPeriodicTxPtrn
{
    /* MAC Address for the adapter */
    tSirMacAddr macAddress;

    /* Bitmap of pattern IDs that need to be deleted */
    tANI_U32 ucPatternIdBitmap;
} tSirDelPeriodicTxPtrn, *tpSirDelPeriodicTxPtrn;

typedef struct sSirRateUpdateInd
{
    /* 0 implies RA, positive value implies fixed rate, -1 implies ignore this
     * param.
     */
    tANI_S32 ucastDataRate;

    /* TX flag to differentiate between HT20, HT40 etc */
    tTxrateinfoflags ucastDataRateTxFlag;

    /* BSSID - Optional. 00-00-00-00-00-00 implies apply to all BCAST STAs */
    tSirMacAddr bssid;

    /*
     * 0 implies MCAST RA, positive value implies fixed rate,
     * -1 implies ignore this param
     */
    tANI_S32 reliableMcastDataRate;//unit Mbpsx10

    /* TX flag to differentiate between HT20, HT40 etc */
    tTxrateinfoflags reliableMcastDataRateTxFlag;

    /*
     * MCAST(or BCAST) fixed data rate in 2.4 GHz, unit Mbpsx10,
     * 0 implies ignore
     */
    tANI_U32 mcastDataRate24GHz;

    /* TX flag to differentiate between HT20, HT40 etc */
    tTxrateinfoflags mcastDataRate24GHzTxFlag;

    /*
     * MCAST(or BCAST) fixed data rate in 5 GHz,
     * unit Mbpsx10, 0 implies ignore
     */
    tANI_U32 mcastDataRate5GHz;

    /* TX flag to differentiate between HT20, HT40 etc */
    tTxrateinfoflags mcastDataRate5GHzTxFlag;

} tSirRateUpdateInd, *tpSirRateUpdateInd;

#ifdef FEATURE_WLAN_BATCH_SCAN
// Set batch scan resposne from FW
typedef struct
{
  /*maximum number of scans which FW can cache*/
  tANI_U32 nScansToBatch;
} tSirSetBatchScanRsp, *tpSirSetBatchScanRsp;

// Set batch scan request to FW
typedef struct
{
    tANI_U32 scanFrequency;        /* how frequent to do scan - default 30Sec*/
    tANI_U32 numberOfScansToBatch; /* number of scans to batch */
    tANI_U32 bestNetwork;          /* best networks in terms of rssi */
    tANI_U8  rfBand;               /* band to scan :
                                      0 ->both Band, 1->2.4Ghz Only
                                      and 2-> 5GHz Only */
    tANI_U32 rtt;                  /* set if required to do RTT it is not
                                      supported in current version */
} tSirSetBatchScanReq, *tpSirSetBatchScanReq;


// Stop batch scan request to FW
typedef struct
{
    tANI_U32 param;
} tSirStopBatchScanInd, *tpSirStopBatchScanInd;

// Trigger batch scan result indication to FW
typedef struct
{
    tANI_U32 param;
} tSirTriggerBatchScanResultInd, *tpSirTriggerBatchScanResultInd;

// Batch scan result indication from FW
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8   bssid[6];     /* BSSID */
    tANI_U8   ssid[33];     /* SSID */
    tANI_U8   ch;           /* Channel */
    tANI_S8   rssi;         /* RSSI or Level */
    /*Timestamp when Network was found. Used to calculate age based on timestamp
      in GET_RSP msg header */
    tANI_U32  timestamp;
} tSirBatchScanNetworkInfo, *tpSirBatchScanNetworkInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   scanId; /* Scan List ID. */
    /*No of AP in a Scan Result. Should be same as bestNetwork in SET_REQ msg*/
    tANI_U32   numNetworksInScanList;
    /*Variable data ptr: Number of AP in Scan List*/
    /*Following numNetworkInScanList is data of type tSirBatchScanNetworkInfo
     *of sizeof(tSirBatchScanNetworkInfo) * numNetworkInScanList */
    tANI_U8    scanList[1];
} tSirBatchScanList, *tpSirBatchScanList;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32      timestamp;
    tANI_U32      numScanLists;
    boolean       isLastResult;
    /* Variable Data ptr: Number of Scan Lists*/
    /* following isLastResult is data of type tSirBatchScanList
     * of sizeof(tSirBatchScanList) * numScanLists*/
    tANI_U8       scanResults[1];
}  tSirBatchScanResultIndParam, *tpSirBatchScanResultIndParam;

#endif // FEATURE_WLAN_BATCH_SCAN

#ifdef FEATURE_WLAN_CH_AVOID
#define SIR_CH_AVOID_MAX_RANGE   4

typedef struct sSirChAvoidFreqType
{
   tANI_U32 startFreq;
   tANI_U32 endFreq;
} tSirChAvoidFreqType;

typedef struct sSirChAvoidIndType
{
   tANI_U32            avoidRangeCount;
   tSirChAvoidFreqType avoidFreqRange[SIR_CH_AVOID_MAX_RANGE];
} tSirChAvoidIndType;
#endif /* FEATURE_WLAN_CH_AVOID */

typedef void (*pGetBcnMissRateCB)( tANI_S32 bcnMissRate,
                                   VOS_STATUS status, void *data);
typedef void (*tSirFWStatsCallback)(VOS_STATUS status, void *fwStatsRsp,
                                                            void *data);
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32   msgLen;
   tANI_U8    bssid[WNI_CFG_BSSID_LEN];
   void      *callback;
   void      *data;
}tSirBcnMissRateReq;

typedef PACKED_PRE struct PACKED_POST
{
    pGetBcnMissRateCB callback;
    void             *data;
}tSirBcnMissRateInfo;

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

typedef struct
{
  u32 reqId;
  tSirMacAddr  macAddr;
  u32 mpduSizeThreshold;
  u32 aggressiveStatisticsGathering;
}tSirLLStatsSetReq, *tpSirLLStatsSetReq;

typedef struct
{
  u32 reqId;
  tSirMacAddr  macAddr;
  u32 paramIdMask;
}tSirLLStatsGetReq, *tpSirLLStatsGetReq;

typedef struct
{
  u32  reqId;
  tSirMacAddr  macAddr;
  u32  statsClearReqMask;
  u8   stopReq;
}tSirLLStatsClearReq, *tpSirLLStatsClearReq;

typedef PACKED_PRE struct PACKED_POST
{
  u32 stats;
  tSirFWStatsCallback callback;
  void *data;
}tSirFWStatsGetReq;

typedef PACKED_PRE struct PACKED_POST
{
  tSirFWStatsCallback callback;
  void *data;
}tSirFWStatsInfo;

/*---------------------------------------------------------------------------
  WLAN_HAL_LL_NOTIFY_STATS
---------------------------------------------------------------------------*/


/******************************LINK LAYER Statistics**********************/

typedef int tSirWifiRadio;
typedef int tSirWifiChannel;
typedef int tSirwifiTxRate;

/* channel operating width */
typedef PACKED_PRE enum PACKED_POST
{
    WIFI_CHAN_WIDTH_20    = 0,
    WIFI_CHAN_WIDTH_40    = 1,
    WIFI_CHAN_WIDTH_80    = 2,
    WIFI_CHAN_WIDTH_160   = 3,
    WIFI_CHAN_WIDTH_80P80 = 4,
    WIFI_CHAN_WIDTH_5     = 5,
    WIFI_CHAN_WIDTH_10    = 6,
} tSirWifiChannelWidth;

typedef PACKED_PRE enum PACKED_POST
{
    WIFI_DISCONNECTED = 0,
    WIFI_AUTHENTICATING = 1,
    WIFI_ASSOCIATING = 2,
    WIFI_ASSOCIATED = 3,
    WIFI_EAPOL_STARTED = 4,   // if done by firmware/driver
    WIFI_EAPOL_COMPLETED = 5, // if done by firmware/driver
} tSirWifiConnectionState;

typedef PACKED_PRE enum PACKED_POST
{
    WIFI_ROAMING_IDLE = 0,
    WIFI_ROAMING_ACTIVE = 1,
} tSirWifiRoamState;

typedef PACKED_PRE enum PACKED_POST
{
    WIFI_INTERFACE_UNKNOWN = -1,
    WIFI_INTERFACE_STA = 0,
    WIFI_INTERFACE_SOFTAP = 1,
    WIFI_INTERFACE_IBSS = 2,
    WIFI_INTERFACE_P2P_CLIENT = 3,
    WIFI_INTERFACE_P2P_GO = 4,
    WIFI_INTERFACE_NAN = 5,
    WIFI_INTERFACE_MESH = 6,
 } tSirWifiInterfaceMode;

// set for QOS association
#define WIFI_CAPABILITY_QOS          0x00000001
// set for protected association (802.11 beacon frame control protected bit set)
#define WIFI_CAPABILITY_PROTECTED    0x00000002
// set if 802.11 Extended Capabilities element interworking bit is set
#define WIFI_CAPABILITY_INTERWORKING 0x00000004
// set for HS20 association
#define WIFI_CAPABILITY_HS20         0x00000008
// set is 802.11 Extended Capabilities element UTF-8 SSID bit is set
#define WIFI_CAPABILITY_SSID_UTF8    0x00000010
// set is 802.11 Country Element is present
#define WIFI_CAPABILITY_COUNTRY      0x00000020

typedef PACKED_PRE struct PACKED_POST
{
    /*tSirWifiInterfaceMode*/
    // interface mode
    tANI_S8                  mode;
    // interface mac address (self)
    tSirMacAddr              macAddr;
    /*tSirWifiConnectionState*/
    // connection state (valid for STA, CLI only)
    tANI_U8                  state;
    /*tSirWifiRoamState*/
    // roaming state
    tANI_U8                  roaming;
    // WIFI_CAPABILITY_XXX (self)
    tANI_U32                 capabilities;
    // null terminated SSID
    tANI_U8                  ssid[33];
    // bssid
    tSirMacAddr              bssid;
    // country string advertised by AP
    tANI_U8                  apCountryStr[WNI_CFG_COUNTRY_CODE_LEN];
    // country string for this association
    tANI_U8                  countryStr[WNI_CFG_COUNTRY_CODE_LEN];
} tSirWifiInterfaceInfo, *tpSirWifiInterfaceInfo;

/* channel information */
typedef PACKED_PRE struct PACKED_POST
{
    // channel width (20, 40, 80, 80+80, 160)
    tSirWifiChannelWidth      width;
    // primary 20 MHz channel
    tSirWifiChannel           centerFreq;
    // center frequency (MHz) first segment
    tSirWifiChannel           centerFreq0;
    // center frequency (MHz) second segment
    tSirWifiChannel           centerFreq1;
} tSirWifiChannelInfo, *tpSirWifiChannelInfo;

/* wifi rate info */
typedef PACKED_PRE struct PACKED_POST
{
    // 0: OFDM, 1:CCK, 2:HT 3:VHT 4..7 reserved
    tANI_U32 preamble   :3;
    // 0:1x1, 1:2x2, 3:3x3, 4:4x4
    tANI_U32 nss        :2;
    // 0:20MHz, 1:40Mhz, 2:80Mhz, 3:160Mhz
    tANI_U32 bw         :3;
    // OFDM/CCK rate code would be as per ieee std in the units of 0.5mbps
    // HT/VHT it would be mcs index
    tANI_U32 rateMcsIdx :8;
    // reserved
    tANI_U32 reserved  :16;
    // units of 100 Kbps
    tANI_U32 bitrate;
} tSirWifiRate, *tpSirWifiRate;

/* channel statistics */
typedef PACKED_PRE struct PACKED_POST
{
    // channel
    tSirWifiChannelInfo channel;
    // msecs the radio is awake (32 bits number accruing over time)
    tANI_U32          onTime;
    // msecs the CCA register is busy (32 bits number accruing over time)
    tANI_U32          ccaBusyTime;
} tSirWifiChannelStats, *tpSirWifiChannelStats;

/* radio statistics */
typedef PACKED_PRE struct PACKED_POST
{
    // wifi radio (if multiple radio supported)
    tSirWifiRadio   radio;
    // msecs the radio is awake (32 bits number accruing over time)
    tANI_U32        onTime;
    /* msecs the radio is transmitting
     * (32 bits number accruing over time)
     */
    tANI_U32        txTime;
    /* msecs the radio is in active receive
     *(32 bits number accruing over time)
     */
    tANI_U32        rxTime;
    /* msecs the radio is awake due to all scan
     * (32 bits number accruing over time)
     */
    tANI_U32        onTimeScan;
    /* msecs the radio is awake due to NAN
     * (32 bits number accruing over time)
     */
    tANI_U32        onTimeNbd;
    /* msecs the radio is awake due to EXTScan
     * (32 bits number accruing over time)
     */
    tANI_U32        onTimeEXTScan;
    /* msecs the radio is awake due to roam?scan
     * (32 bits number accruing over time)
     */
    tANI_U32        onTimeRoamScan;
    /* msecs the radio is awake due to PNO scan
     * (32 bits number accruing over time)
     */
    tANI_U32        onTimePnoScan;
    /* msecs the radio is awake due to HS2.0 scans and GAS exchange
     * (32 bits number accruing over time)
     */
    tANI_U32        onTimeHs20;
    // number of channels
    tANI_U32        numChannels;
    // channel statistics tSirWifiChannelStats
    tSirWifiChannelStats channels[1];
} tSirWifiRadioStat, *tpSirWifiRadioStat;

/* per rate statistics */
typedef PACKED_PRE struct PACKED_POST
{
    // rate information
    tSirWifiRate rate;
    // number of successfully transmitted data pkts (ACK rcvd)
    tANI_U32 txMpdu;
    // number of received data pkts
    tANI_U32 rxMpdu;
    // number of data packet losses (no ACK)
    tANI_U32 mpduLost;
    // total number of data pkt retries *
    tANI_U32 retries;
    // number of short data pkt retries
    tANI_U32 retriesShort;
    // number of long data pkt retries
    tANI_U32 retriesLong;
} tSirWifiRateStat, *tpSirWifiRateStat;

/* access categories */
typedef PACKED_PRE enum PACKED_POST
{
    WIFI_AC_VO  = 0,
    WIFI_AC_VI  = 1,
    WIFI_AC_BE  = 2,
    WIFI_AC_BK  = 3,
    WIFI_AC_MAX = 4,
} tSirWifiTrafficAc;

/* wifi peer type */
typedef PACKED_PRE enum  PACKED_POST
{
    WIFI_PEER_STA,
    WIFI_PEER_AP,
    WIFI_PEER_P2P_GO,
    WIFI_PEER_P2P_CLIENT,
    WIFI_PEER_NAN,
    WIFI_PEER_TDLS,
    WIFI_PEER_INVALID,
} tSirWifiPeerType;

/* per peer statistics */
typedef PACKED_PRE struct PACKED_POST
{
    // peer type (AP, TDLS, GO etc.)
    tSirWifiPeerType type;
    // mac address
    tSirMacAddr    peerMacAddress;
    // peer WIFI_CAPABILITY_XXX
    tANI_U32       capabilities;
    // number of rates
    tANI_U32       numRate;
    // per rate statistics, number of entries  = num_rate
    tSirWifiRateStat rateStats[1];
} tSirWifiPeerInfo, *tpSirWifiPeerInfo;

/* per access category statistics */
typedef PACKED_PRE struct PACKED_POST
{
    /*tSirWifiTrafficAc*/
    // access category (VI, VO, BE, BK)
    tANI_U8 ac;
    // number of successfully transmitted unicast data pkts (ACK rcvd)
    tANI_U32 txMpdu;
    // number of received unicast mpdus
    tANI_U32 rxMpdu;
    // number of succesfully transmitted multicast data packets
    // STA case: implies ACK received from AP for the unicast
    // packet in which mcast pkt was sent
    tANI_U32 txMcast;
    // number of received multicast data packets
    tANI_U32 rxMcast;
    // number of received unicast a-mpdus
    tANI_U32 rxAmpdu;
    // number of transmitted unicast a-mpdus
    tANI_U32 txAmpdu;
    // number of data pkt losses (no ACK)
    tANI_U32 mpduLost;
    // total number of data pkt retries
    tANI_U32 retries;
    // number of short data pkt retries
    tANI_U32 retriesShort;
    // number of long data pkt retries
    tANI_U32 retriesLong;
    // data pkt min contention time (usecs)
    tANI_U32 contentionTimeMin;
    // data pkt max contention time (usecs)
    tANI_U32 contentionTimeMax;
    // data pkt avg contention time (usecs)
    tANI_U32 contentionTimeAvg;
    // num of data pkts used for contention statistics
    tANI_U32 contentionNumSamples;
} tSirWifiWmmAcStat, *tpSirWifiWmmAcStat;

/* Interface statistics - corresponding to 2nd most
 * LSB in wifi statistics bitmap  for getting statistics
 */
typedef PACKED_PRE struct PACKED_POST
{
    // current state of the interface
    tSirWifiInterfaceInfo info;
    // access point beacon received count from connected AP
    tANI_U32            beaconRx;
    // access point mgmt frames received count from
    // connected AP (including Beacon)
    tANI_U32            mgmtRx;
    // action frames received count
    tANI_U32            mgmtActionRx;
    // action frames transmit count
    tANI_U32            mgmtActionTx;
    // access Point Beacon and Management frames RSSI (averaged)
    tANI_S32            rssiMgmt;
    // access Point Data Frames RSSI (averaged) from connected AP
    tANI_S32            rssiData;
    // access Point ACK RSSI (averaged) from connected AP
    tANI_S32            rssiAck;
    // per ac data packet statistics
    tSirWifiWmmAcStat    AccessclassStats[WIFI_AC_MAX];
} tSirWifiIfaceStat, *tpSirWifiIfaceStat;

/* Peer statistics - corresponding to 3rd most LSB in
 * wifi statistics bitmap  for getting statistics
 */
typedef PACKED_PRE struct PACKED_POST
{
    // number of peers
    tANI_U32       numPeers;
    // per peer statistics
    tSirWifiPeerInfo peerInfo[1];
} tSirWifiPeerStat, *tpSirWifiPeerStat;

/* wifi statistics bitmap  for getting statistics */
#define WMI_LINK_STATS_RADIO          0x00000001
#define WMI_LINK_STATS_IFACE          0x00000002
#define WMI_LINK_STATS_ALL_PEER       0x00000004
#define WMI_LINK_STATS_PER_PEER       0x00000008

/* wifi statistics bitmap  for clearing statistics */
// all radio statistics
#define WIFI_STATS_RADIO              0x00000001
// cca_busy_time (within radio statistics)
#define WIFI_STATS_RADIO_CCA          0x00000002
// all channel statistics (within radio statistics)
#define WIFI_STATS_RADIO_CHANNELS     0x00000004
// all scan statistics (within radio statistics)
#define WIFI_STATS_RADIO_SCAN         0x00000008
// all interface statistics
#define WIFI_STATS_IFACE              0x00000010
// all tx rate statistics (within interface statistics)
#define WIFI_STATS_IFACE_TXRATE       0x00000020
// all ac statistics (within interface statistics)
#define WIFI_STATS_IFACE_AC           0x00000040
// all contention (min, max, avg) statistics (within ac statistics)
#define WIFI_STATS_IFACE_CONTENTION   0x00000080

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 paramId;
   tANI_U8  ifaceId;
   tANI_U32 respId;
   tANI_U32 moreResultToFollow;
   tANI_U8  result[1];
}  tSirLLStatsResults, *tpSirLLStatsResults;

#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

#ifdef WLAN_FEATURE_EXTSCAN

typedef enum
{
    WIFI_BAND_UNSPECIFIED,
    WIFI_BAND_BG             = 1,    // 2.4 GHz
    WIFI_BAND_A              = 2,    // 5 GHz without DFS
    WIFI_BAND_ABG            = 3,    // 2.4 GHz + 5 GHz; no DFS
    WIFI_BAND_A_DFS_ONLY     = 4,    // 5 GHz DFS only
    // 5 is reserved
    WIFI_BAND_A_WITH_DFS     = 6,    // 5 GHz with DFS
    WIFI_BAND_ABG_WITH_DFS   = 7,    // 2.4 GHz + 5 GHz with DFS

    /* Keep it last */
    WIFI_BAND_MAX
} tWifiBand;

/* wifi scan related events */
typedef enum
{
   WIFI_SCAN_BUFFER_FULL,
   WIFI_SCAN_COMPLETE,
} tWifiScanEventType;

typedef struct
{
   tSirMacAddr  bssid;   // AP BSSID
   tANI_S32     low;     // low threshold
   tANI_S32     high;    // high threshold
   tANI_U32     channel; // channel hint
} tSirAPThresholdParam, *tpSirAPThresholdParam;

typedef struct
{
    tANI_U32      requestId;
    tANI_U8       sessionId;
} tSirGetEXTScanCapabilitiesReqParams, *tpSirGetEXTScanCapabilitiesReqParams;

typedef struct
{
    tANI_U32      requestId;
    tANI_U32      status;

    tANI_U32      scanCacheSize;
    tANI_U32      scanBuckets;
    tANI_U32      maxApPerScan;
    tANI_U32      maxRssiSampleSize;
    tANI_U32      maxScanReportingThreshold;

    tANI_U32      maxHotlistAPs;
    tANI_U32      maxSignificantWifiChangeAPs;

    tANI_U32      maxBsidHistoryEntries;
} tSirEXTScanCapabilitiesEvent, *tpSirEXTScanCapabilitiesEvent;

/* WLAN_HAL_EXT_SCAN_RESULT_IND */
typedef struct
{
    tANI_U32      requestId;
    tANI_U8       sessionId;

    /*
     * 1 return cached results and flush it
     * 0 return cached results and do not flush
     */
    tANI_BOOLEAN  flush;
} tSirEXTScanGetCachedResultsReqParams, *tpSirEXTScanGetCachedResultsReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32      requestId;
    tANI_U32      status;
} tSirEXTScanGetCachedResultsRspParams, *tpSirEXTScanGetCachedResultsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U64      ts;                          // time of discovery
    tANI_U8       ssid[SIR_MAC_MAX_SSID_LENGTH + 1];   // null terminated SSID
    tSirMacAddr   bssid;                       // BSSID
    tANI_U32      channel;                     // channel frequency in MHz
    tANI_S32      rssi;                        // RSSI in dBm
    tANI_U32      rtt;                         // RTT in nanoseconds
    tANI_U32      rtt_sd;                      // standard deviation in rtt
    tANI_U16      beaconPeriod;       // period advertised in the beacon
    tANI_U16      capability;          // capabilities advertised in the beacon
} tSirWifiScanResult, *tpSirWifiScanResult;

/* WLAN_HAL_BSSID_HOTLIST_RESULT_IND */

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32             requestId;
    tANI_U32             numOfAps;     // numbers of APs

    /*
     * 0 for last fragment
     * 1 still more fragment(s) coming
     */
    tANI_BOOLEAN         moreData;
    tSirWifiScanResult    ap[1];
} tSirWifiScanResultEvent, *tpSirWifiScanResultEvent;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8       elemId;       // Element Identifier
    tANI_U8       ieLength;     // length of IE data
    tANI_U8       *IEs;         // IEs
} tSirInformationElement, *tpSirInformationElement;

/* Reported when each probe response is received, if reportEvents
*  enabled in tSirWifiScanCmdReqParams */
typedef struct
{
    tANI_U32               requestId;

    /*
     * 0 for last fragment
     * 1 still more fragment(s) coming
     */
    tANI_BOOLEAN           moreData;
    tSirWifiScanResult     ap;       // only 1 AP info for now
    tANI_U32               ieLength;
    tSirInformationElement *ie;
} tSirWifiFullScanResultEvent, *tpSirWifiFullScanResultEvent;


typedef struct
{
    tANI_U32      channel;        // frequency
    tANI_U32      dwellTimeMs;    // dwell time hint
    tANI_U8       passive;        // 0 => active,
                                  // 1 => passive scan; ignored for DFS
    tANI_U8       chnlClass;
} tSirWifiScanChannelSpec, *tpSirWifiScanChannelSpec;

typedef struct
{
    tANI_U8       bucket;  // bucket index, 0 based
    tWifiBand     band;    // when UNSPECIFIED, use channel list

    /*
     * desired period, in millisecond; if this is too
     * low, the firmware should choose to generate results as fast as
     * it can instead of failing the command byte
     */
    tANI_U32      period;

    /*
     * 0 => normal reporting (reporting rssi history
     * only, when rssi history buffer is % full)
     * 1 => same as 0 + report a scan completion event after scanning
     * this bucket
     * 2 => same as 1 + forward scan results (beacons/probe responses + IEs)
     * in real time to HAL
     */
    tANI_U8      reportEvents;

    tANI_U8      numChannels;

    /*
     * channels to scan; these may include DFS channels
     */
    tSirWifiScanChannelSpec channels[WLAN_EXTSCAN_MAX_CHANNELS];
} tSirWifiScanBucketSpec, *tpSirWifiScanBucketSpec;

typedef struct
{
    tANI_U32                requestId;
    tANI_U8                 sessionId;
    tANI_U32                basePeriod;   // base timer period
    tANI_U32                maxAPperScan;

    /* in %, when buffer is this much full, wake up host */
    tANI_U32                reportThreshold;

    tANI_U8               numBuckets;
    tSirWifiScanBucketSpec  buckets[WLAN_EXTSCAN_MAX_BUCKETS];
} tSirEXTScanStartReqParams, *tpSirEXTScanStartReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32      requestId;
    tANI_U32      status;
} tSirEXTScanStartRspParams, *tpSirEXTScanStartRspParams;

typedef struct
{
    tANI_U32      requestId;
    tANI_U8       sessionId;
} tSirEXTScanStopReqParams, *tpSirEXTScanStopReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32      requestId;
    tANI_U32      status;
} tSirEXTScanStopRspParams, *tpSirEXTScanStopRspParams;

typedef struct
{
    tANI_U32               requestId;
    tANI_U8                sessionId;    // session Id mapped to vdev_id

    tANI_U32               numAp;        // number of hotlist APs
    tSirAPThresholdParam   ap[WLAN_EXTSCAN_MAX_HOTLIST_APS];    // hotlist APs
} tSirEXTScanSetBssidHotListReqParams, *tpSirEXTScanSetBssidHotListReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32      requestId;
    tANI_U32      status;
} tSirEXTScanSetBssidHotListRspParams, *tpSirEXTScanSetBssidHotListRspParams;

typedef struct
{
    tANI_U32      requestId;
    tANI_U8       sessionId;
} tSirEXTScanResetBssidHotlistReqParams, *tpSirEXTScanResetBssidHotlistReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32      requestId;
    tANI_U32      status;
} tSirEXTScanResetBssidHotlistRspParams, *tpSirEXTScanResetBssidHotlistRspParams;

typedef struct
{
    tANI_U32              requestId;
    tANI_U8               sessionId;

    /* number of samples for averaging RSSI */
    tANI_U32              rssiSampleSize;

    /* number of missed samples to confirm AP loss */
    tANI_U32              lostApSampleSize;

    /* number of APs breaching threshold required for firmware
     * to generate event
     */
    tANI_U32              minBreaching;

    tANI_U32              numAp;
    tSirAPThresholdParam  ap[WLAN_EXTSCAN_MAX_SIGNIFICANT_CHANGE_APS];
} tSirEXTScanSetSignificantChangeReqParams,
 *tpSirEXTScanSetSignificantChangeReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32        requestId;
    tANI_U32        status;
} tSirEXTScanSetSignificantChangeRspParams,
  *tpSirEXTScanSetSignificantChangeRspParams;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SIG_RSSI_RESULT_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tSirMacAddr bssid;                  // BSSID
   tANI_U32  channel;                   // channel frequency in MHz
   tANI_U8   numRssi;                    // number of rssi samples
   tANI_S32   rssi[WLAN_EXTSCAN_MAX_RSSI_SAMPLE_SIZE]; // RSSI history in db
} tSirSigRssiResultParams, *tpSirSigRssiResultParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32     requestId;
    tANI_U32     numSigRssiBss;
    tANI_BOOLEAN moreData;
    tSirSigRssiResultParams sigRssiResult[1];
} tSirWifiSignificantChangeEvent, *tpSirWifiSignificantChangeEvent;

typedef struct
{
    tANI_U32      requestId;
    tANI_U8       sessionId;
} tSirEXTScanResetSignificantChangeReqParams,
  *tpSirEXTScanResetSignificantChangeReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32      requestId;
    tANI_U32      status;
} tSirEXTScanResetSignificantChangeRspParams,
  *tpSirEXTScanResetSignificantChangeRspParams;

/*---------------------------------------------------------------------------
 *  * WLAN_HAL_EXTSCAN_RESULT_AVAILABLE_IND
 *  *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32      requestId;
    tANI_U32      numResultsAvailable;
} tSirEXTScanResultsAvailableIndParams,
  *tpSirEXTScanResultsAvailableIndParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8    scanEventType;
    tANI_U32   status;
} tSirEXTScanOnScanEventIndParams,
  *tpSirEXTScanOnScanEventIndParams;

/*---------------------------------------------------------------------------
 * * WLAN_HAL_EXTSCAN_PROGRESS_IND
 * *-------------------------------------------------------------------------*/

typedef PACKED_PRE enum PACKED_POST
{
    WLAN_HAL_EXTSCAN_BUFFER_FULL,
    WLAN_HAL_EXTSCAN_COMPLETE,
}tSirEXTScanProgressEventType;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 requestId;
    tANI_U32 status;
    tSirEXTScanProgressEventType extScanEventType;
}tSirEXTScanProgressIndParams,
 *tpSirEXTScanProgressIndParams;



#endif /* WLAN_FEATURE_EXTSCAN */

typedef struct
{
    tANI_U16       messageType;
    tANI_U16       length;
    tSirMacAddr    macAddr;
} tSirSpoofMacAddrReq, *tpSirSpoofMacAddrReq;

typedef struct
{
   //BIT order is most likely little endian.
   //This structure is for netowkr-order byte array (or big-endian byte order)
#ifndef WLAN_PAL_BIG_ENDIAN_BIT
   tANI_U8 protVer :2;
   tANI_U8 type :2;
   tANI_U8 subType :4;

   tANI_U8 toDS :1;
   tANI_U8 fromDS :1;
   tANI_U8 moreFrag :1;
   tANI_U8 retry :1;
   tANI_U8 powerMgmt :1;
   tANI_U8 moreData :1;
   tANI_U8 wep :1;
   tANI_U8 order :1;

#else

   tANI_U8 subType :4;
   tANI_U8 type :2;
   tANI_U8 protVer :2;

   tANI_U8 order :1;
   tANI_U8 wep :1;
   tANI_U8 moreData :1;
   tANI_U8 powerMgmt :1;
   tANI_U8 retry :1;
   tANI_U8 moreFrag :1;
   tANI_U8 fromDS :1;
   tANI_U8 toDS :1;

#endif

} tSirFC;

typedef struct
{
   /* Frame control field */
   tSirFC   frameCtrl;
   /* Duration ID */
   tANI_U16 usDurationId;
   /* Address 1 field  */
   tSirMacAddr vA1;
   /* Address 2 field */
   tSirMacAddr vA2;
   /* Address 3 field */
   tSirMacAddr vA3;
   /* Sequence control field */
   tANI_U16 sSeqCtrl;
   /* Optional A4 address */
   tSirMacAddr optvA4;
   /* Optional QOS control field */
   tANI_U16  usQosCtrl;
}tSir80211Header;
// Definition for Encryption Keys
//typedef struct sSirKeys
typedef struct
{
    tANI_U8                  keyId;
    tANI_U8                  unicast;     // 0 for multicast
    tAniKeyDirection    keyDirection;
    tANI_U8                  keyRsc[WLAN_MAX_KEY_RSC_LEN];   // Usage is unknown
    tANI_U8                  paeRole;     // =1 for authenticator,
                                     // =0 for supplicant
    tANI_U16                 keyLength;
    tANI_U8                  key[SIR_MAC_MAX_KEY_LENGTH];
} tMacKeys, *tpMacKeys;

typedef enum
{
    eMAC_WEP_STATIC,
    eMAC_WEP_DYNAMIC,
} tMacWepType;

/*
 * This is used by PE to configure the key information on a given station.
 * When the secType is WEP40 or WEP104, the defWEPIdx is used to locate
 * a preconfigured key from a BSS the station assoicated with; otherwise
 * a new key descriptor is created based on the key field.
 */
//typedef struct
typedef struct
{
    tANI_U16          staIdx;
    tAniEdType        encType;        // Encryption/Decryption type
    tMacWepType       wepType;        // valid only for WEP
    tANI_U8           defWEPIdx;      // Default WEP key, valid only for static WEP, must between 0 and 3
    tMacKeys         key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];            // valid only for non-static WEP encyrptions
    tANI_U8          singleTidRc;    // 1=Single TID based Replay Count, 0=Per TID based RC
    tANI_U8          sessionId; // PE session id for PE<->HAL interface
} tSirSetStaKeyParams, *tpSirSetStaKeyParams;

//typedef struct
typedef struct
{
    tSirSetStaKeyParams keyParams;
    tANI_U8 pn[6];
}tSirencConfigParams;

typedef struct
{
    tANI_U16 length;
    tANI_U8 data[WLAN_DISA_MAX_PAYLOAD_SIZE];
}tSirpayload;

typedef struct
{
    tSir80211Header macHeader;
    tSirencConfigParams encParams;
    tSirpayload data;
}tSirpkt80211;

typedef struct
{
   tANI_U32 status;
   tSirpayload  encryptedPayload;
} tSetEncryptedDataRspParams, *tpSetEncryptedDataRspParams;

typedef struct
{
   tANI_U16   mesgType;
   tANI_U16   mesgLen;
   tSetEncryptedDataRspParams   encryptedDataRsp;
} tSirEncryptedDataRspParams, *tpSirEncryptedDataRspParams;
#endif /* __SIR_API_H */
