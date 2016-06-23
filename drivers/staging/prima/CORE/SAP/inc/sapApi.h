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

#ifndef WLAN_QCT_WLANSAP_H
#define WLAN_QCT_WLANSAP_H

/*===========================================================================

               W L A N   S O F T A P  P A L   L A Y E R 
                       E X T E R N A L  A P I
                
                   
DESCRIPTION
  This file contains the external API exposed by the wlan SAP PAL layer 
  module.
  
      
  Copyright (c) 2010 QUALCOMM Incorporated. All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header: /cygdrive/d/Builds/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT_BTAMP_RSN/CORE/SAP/inc/sapApi.h,v 1.21 2009/03/09 08:58:26 jzmuda Exp jzmuda $ $DateTime: $ $Author: jzmuda $


when           who                what, where, why
--------    ---                 ----------------------------------------------------------
07/01/08     SAP team       Created module.

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "vos_api.h" 
#include "vos_packet.h" 
#include "vos_types.h"

#include "p2p_Api.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
 #ifdef __cplusplus
 extern "C" {
 #endif 
 


/*----------------------------------------------------------------------------
 *  Defines
 * -------------------------------------------------------------------------*/
 
/*--------------------------------------------------------------------------
  defines and enum
  ------------------------------------------------------------------------*/

#define       MAX_SSID_LEN                 32
#define       MAX_ACL_MAC_ADDRESS          16
#define       AUTO_CHANNEL_SELECT          0
#define       MAX_ASSOC_IND_IE_LEN         255

/* defines for WPS config states */
#define       SAP_WPS_DISABLED             0
#define       SAP_WPS_ENABLED_UNCONFIGURED 1
#define       SAP_WPS_ENABLED_CONFIGURED   2

#define       MAX_NAME_SIZE                64
#define       MAX_TEXT_SIZE                32


/*--------------------------------------------------------------------------
  reasonCode take form 802.11 standard Table 7-22 to be passed to WLANSAP_DisassocSta api.
  ------------------------------------------------------------------------*/

typedef enum{  
    eSAP_RC_RESERVED0,               /*0*/  
    eSAP_RC_UNSPECIFIED,             /*1*/  
    eSAP_RC_PREV_AUTH_INVALID,       /*2*/  
    eSAP_RC_STA_LEFT_DEAUTH,         /*3*/  
    eSAP_RC_INACTIVITY_DISASSOC,     /*4*/ 
    eSAP_RC_AP_CAPACITY_FULL,        /*5*/
    eSAP_RC_CLS2_FROM_NON_AUTH_STA,  /*6*/
    eSAP_RC_CLS3_FROM_NON_AUTH_STA,  /*7*/
    eSAP_RC_STA_LEFT_DISASSOC,       /*8*/
    eSAP_RC_STA_NOT_AUTH,            /*9*/
    eSAP_RC_PC_UNACCEPTABLE,         /*10*/
    eSAP_RC_SC_UNACCEPTABLE,         /*11*/
    eSAP_RC_RESERVED1,               /*12*/
    eSAP_RC_INVALID_IE,              /*13*/
    eSAP_RC_MIC_FAIL,                /*14*/
    eSAP_RC_4_WAY_HANDSHAKE_TO,      /*15*/
    eSAP_RC_GO_KEY_HANDSHAKE_TO,     /*16*/
    eSAP_RC_IE_MISMATCH,             /*17*/
    eSAP_RC_INVALID_GRP_CHIPHER,     /*18*/
    eSAP_RC_INVALID_PAIR_CHIPHER,    /*19*/
    eSAP_RC_INVALID_AKMP,            /*20*/
    eSAP_RC_UNSUPPORTED_RSN,         /*21*/
    eSAP_RC_INVALID_RSN,             /*22*/
    eSAP_RC_1X_AUTH_FAILED,          /*23*/
    eSAP_RC_CHIPER_SUITE_REJECTED,   /*24*/
}eSapReasonCode;    

typedef enum {
    eSAP_DOT11_MODE_abg = 0x0001,
    eSAP_DOT11_MODE_11a = 0x0002,
    eSAP_DOT11_MODE_11b = 0x0004,     
    eSAP_DOT11_MODE_11g = 0x0008,     
    eSAP_DOT11_MODE_11n = 0x0010,     
    eSAP_DOT11_MODE_11g_ONLY = 0x0080,
    eSAP_DOT11_MODE_11n_ONLY = 0x0100,
    eSAP_DOT11_MODE_11b_ONLY = 0x0400,
#ifdef WLAN_FEATURE_11AC
    eSAP_DOT11_MODE_11ac     = 0x1000,
    eSAP_DOT11_MODE_11ac_ONLY = 0x2000
#endif
} eSapPhyMode;

typedef enum {
    eSAP_ACCEPT_UNLESS_DENIED = 0,
    eSAP_DENY_UNLESS_ACCEPTED = 1,
    eSAP_SUPPORT_ACCEPT_AND_DENY = 2, /* this type is added to support both accept and deny lists at the same time */
    eSAP_ALLOW_ALL = 3, /*In this mode all MAC addresses are allowed to connect*/
} eSapMacAddrACL;

typedef enum {
    eSAP_BLACK_LIST = 0, /* List of mac addresses NOT allowed to assoc */
    eSAP_WHITE_LIST = 1, /* List of mac addresses allowed to assoc */
} eSapACLType;

typedef enum {
    ADD_STA_TO_ACL      = 0, /* cmd to add STA to access control list */
    DELETE_STA_FROM_ACL = 1, /* cmd to delete STA from access control list */
} eSapACLCmdType;

typedef enum {
    eSAP_START_BSS_EVENT = 0, /*Event sent when BSS is started*/
    eSAP_STOP_BSS_EVENT,      /*Event sent when BSS is stopped*/
    eSAP_STA_ASSOC_IND,       /* Indicate the association request to upper layers */    
    eSAP_STA_ASSOC_EVENT,     /*Event sent when we have successfully associated a station and 
                                upper layer neeeds to allocate a context*/
    eSAP_STA_REASSOC_EVENT,   /*Event sent when we have successfully reassociated a station and 
                                upper layer neeeds to allocate a context*/
    eSAP_STA_DISASSOC_EVENT,  /*Event sent when associated a station has disassociated as a result of various conditions */
    eSAP_STA_SET_KEY_EVENT,   /*Event sent when user called WLANSAP_SetKeySta */
    eSAP_STA_DEL_KEY_EVENT,   /*Event sent when user called WLANSAP_DelKeySta */
    eSAP_STA_MIC_FAILURE_EVENT, /*Event sent whenever there is MIC failure detected */
    eSAP_ASSOC_STA_CALLBACK_EVENT,  /*Event sent when user called WLANSAP_GetAssocStations */
    eSAP_GET_WPSPBC_SESSION_EVENT,  /* Event send when user call  WLANSAP_getWpsSessionOverlap */  
    eSAP_WPS_PBC_PROBE_REQ_EVENT, /* Event send on WPS PBC probe request is received */
    eSAP_INDICATE_MGMT_FRAME,
    eSAP_REMAIN_CHAN_READY,
    eSAP_SEND_ACTION_CNF,
    eSAP_DISCONNECT_ALL_P2P_CLIENT,
    eSAP_MAC_TRIG_STOP_BSS_EVENT,
    eSAP_UNKNOWN_STA_JOIN, /* Event send when a STA in neither white list or black list tries to associate in softap mode */
    eSAP_MAX_ASSOC_EXCEEDED, /* Event send when a new STA is rejected association since softAP max assoc limit has reached */
} eSapHddEvent;

typedef enum {
    eSAP_OPEN_SYSTEM,
    eSAP_SHARED_KEY,
    eSAP_AUTO_SWITCH
 } eSapAuthType; 

typedef enum {
    eSAP_MAC_INITATED_DISASSOC = 0x10000, /*Disassociation was internally initated from CORE stack*/
    eSAP_USR_INITATED_DISASSOC /*Disassociation was internally initated from host by invoking WLANSAP_DisassocSta call*/
 } eSapDisassocReason; 

/*Handle boolean over here*/
typedef enum {
    eSAP_FALSE,
    eSAP_TRUE,
}eSapBool;

/*---------------------------------------------------------------------------
SAP PAL "status" and "reason" error code defines 
 ---------------------------------------------------------------------------*/    
typedef enum  {
    eSAP_STATUS_SUCCESS,                 /* Success.  */
    eSAP_STATUS_FAILURE,                 /* General Failure.  */
    eSAP_START_BSS_CHANNEL_NOT_SELECTED, /* Channel not selected during intial scan.  */
    eSAP_ERROR_MAC_START_FAIL,           /* Failed to start Infra BSS */
}eSapStatus;

/*---------------------------------------------------------------------------
SAP PAL "status" and "reason" error code defines 
 ---------------------------------------------------------------------------*/    
typedef enum  {
    eSAP_WPSPBC_OVERLAP_IN120S,                 /* Overlap */
    eSAP_WPSPBC_NO_WPSPBC_PROBE_REQ_IN120S,     /* no WPS probe request in 120 second */
    eSAP_WPSPBC_ONE_WPSPBC_PROBE_REQ_IN120S,    /* One WPS probe request in 120 second  */
}eWPSPBCOverlap;

typedef enum {
        eSAP_RF_SUBBAND_2_4_GHZ      = 0,
        eSAP_RF_SUBBAND_5_LOW_GHZ    = 1,    //Low & Mid U-NII
        eSAP_RF_SUBBAND_5_MID_GHZ    = 2,    //ETSI
        eSAP_RF_SUBBAND_5_HIGH_GHZ   = 3,    //High U-NII
        eSAP_RF_SUBBAND_4_9_GHZ      = 4,
        eSAP_RF_SUBBAND_5_ALL_GHZ    = 5,    //All 5 GHZ,
}eSapOperatingBand;

/*----------------------------------------------------------------------------
 *  Typedefs
 * -------------------------------------------------------------------------*/
typedef struct sap_StartBssCompleteEvent_s {
    v_U8_t  status;
    v_U8_t  operatingChannel;
    v_U16_t staId; //self StaID
} tSap_StartBssCompleteEvent;

typedef struct sap_StopBssCompleteEvent_s {
    v_U8_t status;
} tSap_StopBssCompleteEvent;

typedef struct sap_StationAssocIndication_s {
    v_MACADDR_t  staMac;
    v_U8_t       assoId;
    v_U8_t       staId;
    v_U8_t       status;
    // Required for indicating the frames to upper layer
    tANI_U32     beaconLength;
    tANI_U8*     beaconPtr;
    tANI_U32     assocReqLength;
    tANI_U8*     assocReqPtr;
    tANI_BOOLEAN fWmmEnabled;
    eCsrAuthType negotiatedAuthType;
    eCsrEncryptionType negotiatedUCEncryptionType;
    eCsrEncryptionType negotiatedMCEncryptionType;
    tANI_BOOLEAN fAuthRequired;
} tSap_StationAssocIndication;

typedef struct sap_StationAssocReassocCompleteEvent_s {
    v_MACADDR_t  staMac;
    v_U8_t       staId;
    v_U8_t       status;
    v_U8_t       ies[MAX_ASSOC_IND_IE_LEN];
    v_U16_t      iesLen;
    v_U32_t      statusCode; 
    eSapAuthType SapAuthType;
    v_BOOL_t     wmmEnabled;
    // Required for indicating the frames to upper layer
    tANI_U32     beaconLength;
    tANI_U8*     beaconPtr;
    tANI_U32     assocReqLength;
    tANI_U8*     assocReqPtr;
    tANI_U32     assocRespLength;
    tANI_U8*     assocRespPtr;    
} tSap_StationAssocReassocCompleteEvent;

typedef struct sap_StationDisassocCompleteEvent_s {
    v_MACADDR_t        staMac;
    v_U8_t             staId;    //STAID should not be used
    v_U8_t             status;
    v_U32_t            statusCode;
    eSapDisassocReason reason;
} tSap_StationDisassocCompleteEvent;

typedef struct sap_StationSetKeyCompleteEvent_s {
    v_U8_t        status;
    v_MACADDR_t   peerMacAddr;
} tSap_StationSetKeyCompleteEvent;

/*struct corresponding to SAP_STA_DEL_KEY_EVENT */
typedef struct sap_StationDeleteKeyCompleteEvent_s {
    v_U8_t status;
    v_U8_t  keyId; /* Key index */
} tSap_StationDeleteKeyCompleteEvent;

/*struct corresponding to SAP_STA_MIC_FAILURE_EVENT */
typedef struct sap_StationMICFailureEvent_s {
    v_MACADDR_t   srcMacAddr; //address used to compute MIC 
    v_MACADDR_t   staMac; //taMacAddr transmitter address
    v_MACADDR_t   dstMacAddr;
    eSapBool   multicast;
    v_U8_t     IV1;            // first byte of IV
    v_U8_t     keyId;          // second byte of IV
    v_U8_t     TSC[SIR_CIPHER_SEQ_CTR_SIZE]; // sequence number

} tSap_StationMICFailureEvent;
/*Structure to return MAC address of associated stations */
typedef struct sap_AssocMacAddr_s {
    v_MACADDR_t staMac;     /*MAC address of Station that is associated*/
    v_U8_t      assocId;        /*Association ID for the station that is associated*/
    v_U8_t      staId;            /*Station Id that is allocated to the station*/
    v_U8_t      ShortGI40Mhz;
    v_U8_t      ShortGI20Mhz;
    v_U8_t      Support40Mhz;
    v_U32_t     requestedMCRate;
    tSirSupportedRates supportedRates;
} tSap_AssocMacAddr, *tpSap_AssocMacAddr;

/*struct corresponding to SAP_ASSOC_STA_CALLBACK_EVENT */
typedef struct sap_AssocStaListEvent_s {
    VOS_MODULE_ID      module; /* module id that was passed in WLANSAP_GetAssocStations API*/
    v_U8_t             noOfAssocSta;  /* Number of associated stations*/
    tpSap_AssocMacAddr pAssocStas; /*Pointer to pre allocated memory to obtain list of associated 
                                    stations passed in WLANSAP_GetAssocStations API*/
} tSap_AssocStaListEvent;
 
typedef struct sap_GetWPSPBCSessionEvent_s {
    v_U8_t         status;
    VOS_MODULE_ID  module; /* module id that was passed in WLANSAP_GetAssocStations API*/       
    v_U8_t         UUID_E[16];         // Unique identifier of the AP.
    v_MACADDR_t    addr;  
    eWPSPBCOverlap wpsPBCOverlap;
} tSap_GetWPSPBCSessionEvent; 

typedef struct sap_WPSPBCProbeReqEvent_s {
    v_U8_t             status;
    VOS_MODULE_ID      module; /* module id that was passed in WLANSAP_GetAssocStations API*/   
    tSirWPSPBCProbeReq WPSPBCProbeReq;
} tSap_WPSPBCProbeReqEvent; 

typedef struct sap_ManagementFrameInfo_s {
    tANI_U32 nFrameLength;
    tANI_U8  frameType;
    tANI_U32 rxChan;            //Channel of where packet is received 
    tANI_U8 *pbFrames;         //Point to a buffer contain the beacon, assoc req, assoc rsp frame, in that order
                             //user needs to use nBeaconLength, nAssocReqLength, nAssocRspLength to desice where
                            //each frame starts and ends.
} tSap_ManagementFrameInfo;

typedef struct sap_SendActionCnf_s {
    eSapStatus actionSendSuccess; 
} tSap_SendActionCnf;

typedef struct sap_UnknownSTAJoinEvent_s {
    v_MACADDR_t    macaddr;  
} tSap_UnknownSTAJoinEvent;

typedef struct sap_MaxAssocExceededEvent_s {
    v_MACADDR_t    macaddr;  
} tSap_MaxAssocExceededEvent;


/* 
   This struct will be filled in and passed to tpWLAN_SAPEventCB that is provided during WLANSAP_StartBss call   
   The event id corresponding to structure  in the union is defined in comment next to the structure
*/

typedef struct sap_Event_s {
    eSapHddEvent sapHddEventCode;
    union {
        tSap_StartBssCompleteEvent                sapStartBssCompleteEvent; /*SAP_START_BSS_EVENT*/
        tSap_StopBssCompleteEvent                 sapStopBssCompleteEvent;  /*SAP_STOP_BSS_EVENT*/
        tSap_StationAssocIndication               sapAssocIndication;       /*SAP_ASSOC_INDICATION */         
        tSap_StationAssocReassocCompleteEvent     sapStationAssocReassocCompleteEvent; /*SAP_STA_ASSOC_EVENT, SAP_STA_REASSOC_EVENT*/
        tSap_StationDisassocCompleteEvent         sapStationDisassocCompleteEvent;/*SAP_STA_DISASSOC_EVENT*/
        tSap_StationSetKeyCompleteEvent           sapStationSetKeyCompleteEvent;/*SAP_STA_SET_KEY_EVENT*/
        tSap_StationDeleteKeyCompleteEvent        sapStationDeleteKeyCompleteEvent;/*SAP_STA_DEL_KEY_EVENT*/
        tSap_StationMICFailureEvent               sapStationMICFailureEvent; /*SAP_STA_MIC_FAILURE_EVENT */
        tSap_AssocStaListEvent                    sapAssocStaListEvent; /*SAP_ASSOC_STA_CALLBACK_EVENT */
        tSap_GetWPSPBCSessionEvent                sapGetWPSPBCSessionEvent; /*SAP_GET_WPSPBC_SESSION_EVENT */
        tSap_WPSPBCProbeReqEvent                  sapPBCProbeReqEvent; /*eSAP_WPS_PBC_PROBE_REQ_EVENT */
        tSap_ManagementFrameInfo                  sapManagementFrameInfo; /*eSAP_INDICATE_MGMT_FRAME*/
        tSap_SendActionCnf                        sapActionCnf;  /* eSAP_SEND_ACTION_CNF */ 
        tSap_UnknownSTAJoinEvent                  sapUnknownSTAJoin; /* eSAP_UNKNOWN_STA_JOIN */
        tSap_MaxAssocExceededEvent                sapMaxAssocExceeded; /* eSAP_MAX_ASSOC_EXCEEDED */
    } sapevt;
} tSap_Event, *tpSap_Event;


typedef __ani_attr_pre_packed struct sap_SSID {
    v_U8_t       length;
    v_U8_t       ssId[MAX_SSID_LEN];
} __ani_attr_packed tSap_SSID_t;

typedef __ani_attr_pre_packed struct sap_SSIDInfo {
    tSap_SSID_t  ssid;       /*SSID of the AP*/
    v_U8_t       ssidHidden; /*SSID shouldn't/should be broadcast in probe RSP and beacon*/
} __ani_attr_packed tSap_SSIDInfo_t;

typedef struct sap_Config {
    tSap_SSIDInfo_t SSIDinfo;
    eSapPhyMode     SapHw_mode; /* Wireless Mode */
    eSapMacAddrACL  SapMacaddr_acl;
    v_MACADDR_t     accept_mac[MAX_ACL_MAC_ADDRESS]; /* MAC filtering */
    v_BOOL_t        ieee80211d;      /*Specify if 11D is enabled or disabled*/
    v_BOOL_t        protEnabled;     /*Specify if protection is enabled or disabled*/
    v_BOOL_t        obssProtEnabled; /*Specify if OBSS protection is enabled or disabled*/
    v_MACADDR_t     deny_mac[MAX_ACL_MAC_ADDRESS]; /* MAC filtering */
    v_MACADDR_t     self_macaddr; //self macaddress or BSSID
   
    v_U8_t          channel;         /* Operation channel */
    v_U8_t          max_num_sta;     /* maximum number of STAs in station table */
    v_U8_t          dtim_period;     /* dtim interval */
    v_U8_t          num_accept_mac;
    v_U8_t          num_deny_mac;
    v_U8_t          *pRSNWPAReqIE;   //If not null, it has the IE byte stream for RSN /WPA

    v_U8_t          countryCode[WNI_CFG_COUNTRY_CODE_LEN];  //it is ignored if [0] is 0.
    v_U8_t          RSNAuthType;
    v_U8_t          RSNEncryptType;
    v_U8_t          mcRSNEncryptType;
    eSapAuthType    authType;
    v_BOOL_t        privacy;
    v_BOOL_t        UapsdEnable;
    v_BOOL_t        fwdWPSPBCProbeReq;
    v_U8_t          wps_state; // 0 - disabled, 1 - not configured , 2 - configured

    v_U16_t         ht_capab;
    v_U16_t         RSNWPAReqIELength;   //The byte count in the pWPAReqIE

    v_U32_t         beacon_int;     /* Beacon Interval */
    v_U32_t         ap_table_max_size;
    v_U32_t         ap_table_expiration_time;
    v_U32_t         ht_op_mode_fixed;
    tVOS_CON_MODE   persona; /*Tells us which persona it is GO or AP for now*/

#ifdef WLAN_FEATURE_11W
    v_BOOL_t        mfpRequired;
    v_BOOL_t        mfpCapable;
#endif
    eCsrBand        scanBandPreference;
    v_U16_t         acsBandSwitchThreshold;

} tsap_Config_t;

typedef enum {
    eSAP_WPS_PROBE_RSP_IE,
    eSAP_WPS_BEACON_IE,
    eSAP_WPS_ASSOC_RSP_IE
} eSapWPSIE_CODE;

typedef struct sSapName {
    v_U8_t num_name;
    v_U8_t name[MAX_NAME_SIZE];
} tSapName;

typedef struct sSapText {
    v_U8_t num_text;
    v_U8_t text[MAX_TEXT_SIZE];
} tSapText;

#define WPS_PROBRSP_VER_PRESENT                          0x00000001
#define WPS_PROBRSP_STATE_PRESENT                        0x00000002
#define WPS_PROBRSP_APSETUPLOCK_PRESENT                  0x00000004
#define WPS_PROBRSP_SELECTEDREGISTRA_PRESENT             0x00000008
#define WPS_PROBRSP_DEVICEPASSWORDID_PRESENT             0x00000010
#define WPS_PROBRSP_SELECTEDREGISTRACFGMETHOD_PRESENT    0x00000020
#define WPS_PROBRSP_RESPONSETYPE_PRESENT                 0x00000040
#define WPS_PROBRSP_UUIDE_PRESENT                        0x00000080
#define WPS_PROBRSP_MANUFACTURE_PRESENT                  0x00000100
#define WPS_PROBRSP_MODELNAME_PRESENT                    0x00000200
#define WPS_PROBRSP_MODELNUMBER_PRESENT                  0x00000400
#define WPS_PROBRSP_SERIALNUMBER_PRESENT                 0x00000800
#define WPS_PROBRSP_PRIMARYDEVICETYPE_PRESENT            0x00001000
#define WPS_PROBRSP_DEVICENAME_PRESENT                   0x00002000
#define WPS_PROBRSP_CONFIGMETHODS_PRESENT                0x00004000
#define WPS_PROBRSP_RF_BANDS_PRESENT                     0x00008000

typedef struct sap_WPSProbeRspIE_s {
    v_U32_t     FieldPresent;
    v_U32_t     Version;           // Version. 0x10 = version 1.0, 0x11 = etc.
    v_U32_t     wpsState;          // 1 = unconfigured, 2 = configured.    
    v_BOOL_t    APSetupLocked;     // Must be included if value is TRUE
    v_BOOL_t    SelectedRegistra;  //BOOL:  indicates if the user has recently activated a Registrar to add an Enrollee.
    v_U16_t     DevicePasswordID;  // Device Password ID
    v_U16_t     SelectedRegistraCfgMethod; // Selected Registrar config method
    v_U8_t      ResponseType;      // Response type
    v_U8_t      UUID_E[16];         // Unique identifier of the AP.
    tSapName    Manufacture;
    tSapText    ModelName;
    tSapText    ModelNumber;
    tSapText    SerialNumber;
    v_U32_t     PrimaryDeviceCategory ; // Device Category ID: 1Computer, 2Input Device, ...
    v_U8_t      PrimaryDeviceOUI[4] ; // Vendor specific OUI for Device Sub Category
    v_U32_t     DeviceSubCategory ; // Device Sub Category ID: 1-PC, 2-Server if Device Category ID is computer
    tSapText    DeviceName;
    v_U16_t    ConfigMethod;     // Configuaration method
    v_U8_t    RFBand;           // RF bands available on the AP
} tSap_WPSProbeRspIE;

#define WPS_BEACON_VER_PRESENT                         0x00000001
#define WPS_BEACON_STATE_PRESENT                       0x00000002
#define WPS_BEACON_APSETUPLOCK_PRESENT                 0x00000004
#define WPS_BEACON_SELECTEDREGISTRA_PRESENT            0x00000008
#define WPS_BEACON_DEVICEPASSWORDID_PRESENT            0x00000010
#define WPS_BEACON_SELECTEDREGISTRACFGMETHOD_PRESENT   0x00000020
#define WPS_BEACON_UUIDE_PRESENT                       0x00000080
#define WPS_BEACON_RF_BANDS_PRESENT                    0x00000100

typedef struct sap_WPSBeaconIE_s {
    v_U32_t  FieldPresent;
    v_U32_t  Version;           // Version. 0x10 = version 1.0, 0x11 = etc.
    v_U32_t  wpsState;          // 1 = unconfigured, 2 = configured.    
    v_BOOL_t APSetupLocked;     // Must be included if value is TRUE
    v_BOOL_t SelectedRegistra;  //BOOL:  indicates if the user has recently activated a Registrar to add an Enrollee.
    v_U16_t  DevicePasswordID;  // Device Password ID
    v_U16_t  SelectedRegistraCfgMethod; // Selected Registrar config method
    v_U8_t   UUID_E[16];        // Unique identifier of the AP.
    v_U8_t   RFBand;           // RF bands available on the AP
} tSap_WPSBeaconIE;

#define WPS_ASSOCRSP_VER_PRESENT             0x00000001
#define WPS_ASSOCRSP_RESPONSETYPE_PRESENT    0x00000002

typedef struct sap_WPSAssocRspIE_s {
   v_U32_t FieldPresent;
   v_U32_t Version;
   v_U8_t ResposeType;
} tSap_WPSAssocRspIE;

typedef struct sap_WPSIE_s {
    eSapWPSIE_CODE sapWPSIECode;
    union {
               tSap_WPSProbeRspIE  sapWPSProbeRspIE;    /*WPS Set Probe Respose IE*/
               tSap_WPSBeaconIE    sapWPSBeaconIE;      /*WPS Set Beacon IE*/
               tSap_WPSAssocRspIE  sapWPSAssocRspIE;    /*WPS Set Assoc Response IE*/
    } sapwpsie;
} tSap_WPSIE, *tpSap_WPSIE;

#ifdef WLANTL_DEBUG
#define MAX_RATE_INDEX      136
#define MAX_NUM_RSSI        100
#define MAX_RSSI_INTERVAL     5
#endif

typedef struct sap_SoftapStats_s {
   v_U32_t txUCFcnt;
   v_U32_t txMCFcnt;
   v_U32_t txBCFcnt;
   v_U32_t txUCBcnt;
   v_U32_t txMCBcnt;
   v_U32_t txBCBcnt;
   v_U32_t rxUCFcnt;
   v_U32_t rxMCFcnt;
   v_U32_t rxBCFcnt;
   v_U32_t rxUCBcnt;
   v_U32_t rxMCBcnt;
   v_U32_t rxBCBcnt;
   v_U32_t rxBcnt;
   v_U32_t rxBcntCRCok;
   v_U32_t rxRate;
#ifdef WLANTL_DEBUG
   v_U32_t pktCounterRateIdx[MAX_RATE_INDEX];
   v_U32_t pktCounterRssi[MAX_NUM_RSSI];
#endif
} tSap_SoftapStats, *tpSap_SoftapStats;

#ifdef FEATURE_WLAN_CH_AVOID
/* Store channel safty information */
typedef struct
{
   v_U16_t   channelNumber;
   v_BOOL_t  isSafe;
} safeChannelType;
#endif /* FEATURE_WLAN_CH_AVOID */

int sapSetPreferredChannel(tANI_U8* ptr);
void sapCleanupChannelList(void);

/*==========================================================================
  FUNCTION    WLANSAP_Set_WpsIe

  DESCRIPTION 
    This api function provides for Ap App/HDD to set WPS IE.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
  pvosGCtx: Pointer to vos global context structure
  pWPSIE:  tSap_WPSIE structure for the station
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_Set_WpsIe
(
    v_PVOID_t pvosGCtx, tSap_WPSIE *pWPSIe
);

/*==========================================================================
  FUNCTION   WLANSAP_Update_WpsIe

  DESCRIPTION 
    This api function provides for Ap App/HDD to start WPS session.

  DEPENDENCIES
    NA. 

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_Update_WpsIe
(
    v_PVOID_t pvosGCtx
);

/*==========================================================================
  FUNCTION    WLANSAP_Stop_Wps

  DESCRIPTION 
    This api function provides for Ap App/HDD to stop WPS session.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_Stop_Wps
(
    v_PVOID_t pvosGCtx
);

/*==========================================================================
  FUNCTION    WLANSAP_Get_WPS_State

  DESCRIPTION 
    This api function provides for Ap App/HDD to get WPS state.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure

    OUT
pbWPSState: Pointer to variable to indicate if it is in WPS Registration state
 
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_Get_WPS_State
(
    v_PVOID_t pvosGCtx, v_BOOL_t * pbWPSState
); 

/*----------------------------------------------------------------------------
 *  Opaque SAP handle Type Declaration 
 * -------------------------------------------------------------------------*/

typedef v_PVOID_t tSapHandle, *ptSapHandle;

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/*==========================================================================
  FUNCTION    WLANSAP_Open

  DESCRIPTION 
    Called at driver initialization (vos_open). SAP will initialize 
    all its internal resources and will wait for the call to start to 
    register with the other modules. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
  pvosGCtx:       pointer to the global vos context; a handle to SAP's 
                  control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to SAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_Open
( 
    v_PVOID_t  pvosGCtx 
);

/*==========================================================================
  FUNCTION    WLANSAP_Start

  DESCRIPTION 
    Called as part of the overall start procedure (vos_start). 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to SAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to SAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

    Other codes can be returned as a result of a BAL failure;
    
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_Start
( 
    v_PVOID_t  pvosGCtx 
);

/*==========================================================================
  FUNCTION    WLANSAP_Stop

  DESCRIPTION 
    Called by vos_stop to stop operation in SAP, before close. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to SAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to SAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_Stop
( 
    v_PVOID_t  pvosGCtx 
);

/*==========================================================================
  FUNCTION    WLANSAP_Close

  DESCRIPTION 
    Called by vos_close during general driver close procedure. SAP will clean up 
    all the internal resources. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to SAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to SAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_Close
( 
    v_PVOID_t  pvosGCtx 
);

/*==========================================================================
  FUNCTION    (*tpWLAN_SAPEventCB)

  DESCRIPTION 
    Implements the callback for ALL asynchronous events. 
    Including Events resulting from:
     * Start BSS 
     * Stop BSS,...

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    pSapEvent:  pointer to the union of "Sap Event" structures. This now encodes ALL event types.
        Including Command Complete and Command Status
    pUsrContext   : pUsrContext parameter that was passed to sapStartBss
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pSapEvent is NULL
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
typedef VOS_STATUS (*tpWLAN_SAPEventCB)( tpSap_Event pSapEvent, v_PVOID_t  pUsrContext);



/*==========================================================================
  FUNCTION    WLANSAP_getState

  DESCRIPTION 
    This api returns the current SAP state to the caller.

  DEPENDENCIES 

  PARAMETERS 

    IN
    pContext            : Pointer to Sap Context structure

  RETURN VALUE
    Returns the SAP FSM state.  
============================================================================*/

v_U8_t WLANSAP_getState ( v_PVOID_t  pvosGCtx);

/*==========================================================================
  FUNCTION    WLANSAP_StartBss

  DESCRIPTION 
    This api function provides SAP FSM event eWLAN_SAP_HDD_PHYSICAL_LINK_CREATE for
starting AP BSS 

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
pvosGCtx: Pointer to vos global context structure
pConfig: Pointer to configuration structure passed down from HDD(HostApd for Android)
hdd_SapEventCallback: Callback function in HDD called by SAP to inform HDD about SAP results
usrDataForCallback: Parameter that will be passed back in all the SAP callback events.

   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_StartBss
(
    v_PVOID_t  pvosGCtx, 
    tpWLAN_SAPEventCB pSapEventCallback, 
    tsap_Config_t *pConfig, v_PVOID_t  pUsrContext
);

/*==========================================================================
  FUNCTION    WLANSAP_SetMacACL

  DESCRIPTION
  This api function provides SAP to set mac list entry in accept list as well
  as deny list

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
pConfig:  Pointer to configuration structure passed down from
          HDD(HostApd for Android)


  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_SetMacACL
(
    v_PVOID_t  pvosGCtx,
    tsap_Config_t *pConfig
);

/*==========================================================================
  FUNCTION    WLANSAP_Stop

  DESCRIPTION 
    This api function provides SAP FSM event eWLAN_SAP_HDD_PHYSICAL_LINK_DISCONNECT for
stopping BSS 

  DEPENDENCIES 
    NA. 

  PARAMETERS W

    IN
    pvosGCtx: Pointer to vos global context structure
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_StopBss
(
    v_PVOID_t  pvosGCtx
);

/*==========================================================================
  FUNCTION    WLANSAP_DisassocSta

  DESCRIPTION 
    This api function provides for Ap App/HDD initiated disassociation of station

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
    pvosGCtx            : Pointer to vos global context structure
    pPeerStaMac         : Mac address of the station to disassociate
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_DisassocSta
(
    v_PVOID_t  pvosGCtx, v_U8_t *pPeerStaMac
);

/*==========================================================================
  FUNCTION    WLANSAP_DeauthSta

  DESCRIPTION 
    This api function provides for Ap App/HDD initiated deauthentication of station

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
    pvosGCtx            : Pointer to vos global context structure
    pDelStaParams       : Pointer to parameters of the station to
                          deauthenticate
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_DeauthSta
(
    v_PVOID_t  pvosGCtx,
    struct tagCsrDelStaParams *pDelStaParams
);

/*==========================================================================
  FUNCTION    WLANSAP_SetChannelRange

  DESCRIPTION 
      This api function sets the range of channels for SoftAP.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
    startChannel         : start channel
    endChannel           : End channel
    operatingBand        : Operating band (2.4GHz/5GHz)
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_SetChannelRange(tHalHandle hHal,v_U8_t startChannel, v_U8_t endChannel, 
                              eSapOperatingBand operatingBand);

/*==========================================================================
  FUNCTION    WLANSAP_SetKeySta

  DESCRIPTION 
    This api function provides for Ap App/HDD to delete key for a station.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
pSetKeyInfo: tCsrRoamSetKey structure for the station
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_SetKeySta
(
    v_PVOID_t pvosGCtx, tCsrRoamSetKey *pSetKeyInfo
); 

/*==========================================================================
  FUNCTION    WLANSAP_DelKeySta

  DESCRIPTION 
    This api function provides for Ap App/HDD to delete key for a station.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
pSetKeyInfo: tCsrRoamSetKey structure for the station
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_DelKeySta
(
    v_PVOID_t pvosGCtx, tCsrRoamRemoveKey *pDelKeyInfo
); 



/*==========================================================================
  FUNCTION    WLANSAP_GetAssocStations

  DESCRIPTION 
    This api function is used to probe the list of associated stations from various modules of CORE stack

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
mod: Module from whom list of associtated stations  is supposed to be probed. If an invalid module is passed
then by default VOS_MODULE_ID_PE will be probed
    IN/OUT
pNoOfAssocStas:- Number of  associated stations that are known to the module specified in mod parameter
pAssocStas: Pointer to list of associated stations that are known to the module specified in mod parameter 
NOTE:- The memory for this list will be allocated by the caller of this API
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_GetAssocStations
(
    v_PVOID_t pvosGCtx, VOS_MODULE_ID module, 
    tpSap_AssocMacAddr pAssocStas
); 
/*==========================================================================
  FUNCTION    WLANSAP_RemoveWpsSessionOverlap

  DESCRIPTION 
    This api function provides for Ap App/HDD to remove an entry from session session overlap info.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
    pvosGCtx: Pointer to vos global context structure
    pRemoveMac: pointer to v_MACADDR_t for session MAC address that needs to be removed from wps session
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
    VOS_STATUS_E_FAULT:  Session is not dectected. The parameter is function not valid.
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_RemoveWpsSessionOverlap

(
    v_PVOID_t pvosGCtx,
    v_MACADDR_t pRemoveMac
);

/*==========================================================================
  FUNCTION    WLANSAP_getWpsSessionOverlap

  DESCRIPTION 
    This api function provides for Ap App/HDD to get WPS session overlap info.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
pSessionMac: pointer to v_MACADDR_t for session MAC address
uuide: Pointer to 16 bytes array for session UUID_E
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
    VOS_STATUS_E_FAULT:  Overlap is dectected. The parameter is function not valid.
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_getWpsSessionOverlap
(
    v_PVOID_t pvosGCtx
);

/*==========================================================================
  FUNCTION    WLANSAP_SetCounterMeasure

  DESCRIPTION 
    This api function is used to disassociate all the stations and prevent 
    association for any other station.Whenever Authenticator receives 2 mic failures 
    within 60 seconds, Authenticator will enable counter measure at SAP Layer. 
    Authenticator will start the 60 seconds timer. Core stack will not allow any 
    STA to associate till HDD disables counter meassure. Core stack shall kick out all the 
    STA which are currently associated and DIASSOC Event will be propogated to HDD for 
    each STA to clean up the HDD STA table.Once the 60 seconds timer expires, Authenticator 
    will disable the counter meassure at core stack. Now core stack can allow STAs to associate.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
bEnable: If TRUE than all stations will be disassociated and no more will be allowed to associate. If FALSE than CORE
will come out of this state.
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSAP_SetCounterMeasure
(
    v_PVOID_t pvosGCtx, v_BOOL_t bEnable
);

/*==========================================================================
  FUNCTION    WLANSap_getstationIE_information

  DESCRIPTION 
    This api function provides for Ap App/HDD to retrive the WPA and RSNIE of a station.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
        pvosGCtx: Pointer to vos global context structure
        pLen : length of WPARSN elment IE where it would be copied
        pBuf : buf to copy the WPARSNIe 
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS
WLANSap_getstationIE_information(v_PVOID_t pvosGCtx, 
                                 v_U32_t   *pLen,
                                 v_U8_t    *pBuf);


VOS_STATUS
WLANSAP_getWpsSessionOverlap
(
    v_PVOID_t pvosGCtx
);
/*==========================================================================
  FUNCTION    WLANSAP_ClearACL

  DESCRIPTION 
    This api function removes all the entries in both accept and deny lists.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
        pvosGCtx: Pointer to vos global context structure
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_ClearACL
( 
    v_PVOID_t  pvosGCtx
);

/*==========================================================================
  FUNCTION    WLANSAP_SetMode

  DESCRIPTION 
    This api is used to set mode for ACL

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
        pvosGCtx: Pointer to vos global context structure
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_SetMode
( 
    v_PVOID_t  pvosGCtx,
    v_U32_t mode
);

/*==========================================================================
  FUNCTION    WLANSAP_ModifyACL

  DESCRIPTION 
    This api function provides for Ap App/HDD to add/remove mac addresses from black/white lists (ACLs).

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
        pvosGCtx        : Pointer to vos global context structure
        pPeerStaMac     : MAC address to be added or removed 
        listType        : add/remove to be done on black or white list
        cmd             : Are we doing to add or delete a mac addr from an ACL.
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS 
WLANSAP_ModifyACL
(
    v_PVOID_t  pvosGCtx,
    v_U8_t *pPeerStaMac,
    eSapACLType listType,
    eSapACLCmdType cmd
);

/*==========================================================================
  FUNCTION    WLANSAP_Set_WPARSNIes

  DESCRIPTION 
    This api function provides for Ap App/HDD to set AP WPA and RSN IE in its beacon and probe response.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
        pvosGCtx: Pointer to vos global context structure
        pWPARSNIEs: buffer to the WPA/RSN IEs 
        WPARSNIEsLen: length of WPA/RSN IEs
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS WLANSAP_Set_WPARSNIes(v_PVOID_t pvosGCtx, v_U8_t *pWPARSNIEs, v_U32_t WPARSNIEsLen);

/*==========================================================================
  FUNCTION    WLANSAP_GetStatistics

  DESCRIPTION 
    This api function provides for Ap App/HDD to get TL statistics for all stations of Soft AP.

  DEPENDENCIES 
    NA. 

  PARAMETERS

    IN
        pvosGCtx: Pointer to vos global context structure
        bReset: If set TL statistics will be cleared after reading
    OUT
        statBuf: Buffer to get the statistics

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS   
============================================================================*/
VOS_STATUS WLANSAP_GetStatistics(v_PVOID_t pvosGCtx, tSap_SoftapStats *statBuf, v_BOOL_t bReset);

/*==========================================================================

  FUNCTION    WLANSAP_SendAction

  DESCRIPTION 
    This api function provides to send action frame sent by upper layer.

  DEPENDENCIES 
    NA. 

  PARAMETERS

  IN
    pvosGCtx: Pointer to vos global context structure
    pBuf: Pointer of the action frame to be transmitted
    len: Length of the action frame
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS WLANSAP_SendAction( v_PVOID_t pvosGCtx, const tANI_U8 *pBuf, 
                               tANI_U32 len, tANI_U16 wait );

/*==========================================================================

  FUNCTION    WLANSAP_RemainOnChannel

  DESCRIPTION 
    This api function provides to set Remain On channel on specified channel
    for specified duration.

  DEPENDENCIES 
    NA. 

  PARAMETERS

  IN
    pvosGCtx: Pointer to vos global context structure
    channel: Channel on which driver has to listen 
    duration: Duration for which driver has to listen on specified channel
    callback: Callback function to be called once Listen is done.
    pContext: Context needs to be called in callback function. 
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS WLANSAP_RemainOnChannel( v_PVOID_t pvosGCtx,
                                    tANI_U8 channel, tANI_U32 duration,
                                    remainOnChanCallback callback, 
                                    void *pContext );

/*==========================================================================

  FUNCTION    WLANSAP_CancelRemainOnChannel

  DESCRIPTION 
    This api cancel previous remain on channel request.

  DEPENDENCIES 
    NA. 

  PARAMETERS

  IN
    pvosGCtx: Pointer to vos global context structure
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS WLANSAP_CancelRemainOnChannel( v_PVOID_t pvosGCtx );


/*==========================================================================

  FUNCTION    WLANSAP_RegisterMgmtFrame

  DESCRIPTION 
    HDD use this API to register specified type of frame with CORE stack.
    On receiving such kind of frame CORE stack should pass this frame to HDD

  DEPENDENCIES 
    NA. 

  PARAMETERS

  IN
    pvosGCtx: Pointer to vos global context structure
    frameType: frameType that needs to be registered with PE.
    matchData: Data pointer which should be matched after frame type is matched.
    matchLen: Length of the matchData
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS WLANSAP_RegisterMgmtFrame( v_PVOID_t pvosGCtx, tANI_U16 frameType, 
                                      tANI_U8* matchData, tANI_U16 matchLen );

/*==========================================================================

  FUNCTION    WLANSAP_DeRegisterMgmtFrame

  DESCRIPTION 
   This API is used to deregister previously registered frame. 

  DEPENDENCIES 
    NA. 

  PARAMETERS

  IN
    pvosGCtx: Pointer to vos global context structure
    frameType: frameType that needs to be De-registered with PE.
    matchData: Data pointer which should be matched after frame type is matched.
    matchLen: Length of the matchData
   
  RETURN VALUE
    The VOS_STATUS code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS   
============================================================================*/
VOS_STATUS WLANSAP_DeRegisterMgmtFrame( v_PVOID_t pvosGCtx, tANI_U16 frameType, 
                                      tANI_U8* matchData, tANI_U16 matchLen );


/*==========================================================================
  FUNCTION    WLANSAP_PopulateDelStaParams

  DESCRIPTION
  This API is used to populate del station parameters
  DEPENDENCIES
  NA.

  PARAMETERS
  IN
  mac:           pointer to peer mac address.
  reason_code:   Reason code for the disassoc/deauth.
  subtype:       subtype points to either disassoc/deauth frame.
  pDelStaParams: address where parameters to be populated.

  RETURN VALUE NONE

  SIDE EFFECTS
============================================================================*/

void WLANSAP_PopulateDelStaParams(const v_U8_t *mac,
                                 v_U16_t reason_code,
                                 v_U8_t subtype,
                                 struct tagCsrDelStaParams *pDelStaParams);

#ifdef __cplusplus
 }
#endif 


#endif /* #ifndef WLAN_QCT_WLANSAP_H */

