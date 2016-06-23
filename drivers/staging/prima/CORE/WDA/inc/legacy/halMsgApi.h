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


#ifndef _HALMSGAPI_H_
#define _HALMSGAPI_H_

#include "halTypes.h"
#include "sirApi.h"
#include "sirParams.h"

#define HAL_NUM_BSSID 2
/* operMode in ADD BSS message */
#define BSS_OPERATIONAL_MODE_AP     0
#define BSS_OPERATIONAL_MODE_STA    1

/* STA entry type in add sta message */
#define STA_ENTRY_SELF              0
#define STA_ENTRY_OTHER             1
#define STA_ENTRY_BSSID             2
#define STA_ENTRY_BCAST             3 //Special station id for transmitting broadcast frames.
#define STA_ENTRY_PEER              STA_ENTRY_OTHER
#ifdef FEATURE_WLAN_TDLS
#define STA_ENTRY_TDLS_PEER         4
#endif /* FEATURE_WLAN_TDLS */

#define STA_ENTRY_TRANSMITTER       STA_ENTRY_SELF
#define STA_ENTRY_RECEIVER          STA_ENTRY_OTHER

#define HAL_STA_INVALID_IDX 0xFF
#define HAL_BSS_INVALID_IDX 0xFF

#define HAL_BSSPERSONA_INVALID_IDX 0xFF

#define WLAN_BSS_PROTECTION_ON  1
#define WLAN_BSS_PROTECTION_OFF 0

/* Station index allocation after Broadcast station */
#define HAL_MAX_NUM_BCAST_STATIONS      1
#define HAL_MIN_BCAST_STA_INDEX     ((HAL_MAX_NUM_BCAST_STATIONS>0)?0:HAL_STA_INVALID_IDX)
#define HAL_MAX_BCAST_STA_INDEX     ((HAL_MAX_NUM_BCAST_STATIONS>0)?(HAL_MAX_NUM_BCAST_STATIONS - 1):HAL_STA_INVALID_IDX)
#define HAL_MIN_STA_INDEX           ((HAL_MAX_BCAST_STA_INDEX!=HAL_STA_INVALID_IDX)?(HAL_MAX_BCAST_STA_INDEX+1):0)
#define HAL_MAX_STA_INDEX           (HAL_NUM_STA)

/* Compilation flags for enabling disabling selfSta and bcastSta per BSS */
#define HAL_SELF_STA_PER_BSS        1
#define HAL_BCAST_STA_PER_BSS       1

//invalid channel id.
#define HAL_INVALID_CHANNEL_ID 0

/* BSS index used when no BSS is associated with the station. For example,
 * driver creates only one self station without valid BSS while scanning.
 * Then this index is used to tell softmac that BSS is not valid.
 */
#define BSSIDX_INVALID             254

#define HAL_IS_VALID_BSS_INDEX(pMac, bssIdx)  ((BSSIDX_INVALID != (bssIdx)) && ((bssIdx) < (pMac)->hal.memMap.maxBssids))

// Beacon structure
typedef __ani_attr_pre_packed struct sAniBeaconStruct
{
    tANI_U32        beaconLength;   // Indicates the beacon length
    tSirMacMgmtHdr  macHdr;         // MAC Header for beacon
    // Beacon body follows here
} __ani_attr_packed tAniBeaconStruct, *tpAniBeaconStruct;

// probeRsp template structure
typedef __ani_attr_pre_packed struct sAniProbeRspStruct
{
    tSirMacMgmtHdr  macHdr;         // MAC Header for probeRsp
    // probeRsp body follows here
} __ani_attr_packed tAniProbeRspStruct, *tpAniProbeRspStruct;


// Per TC parameters
typedef struct
{
    tANI_U8 disableTx;
    tANI_U8 disableRx;
    tANI_U8 rxCompBA;              // 1: expect to see frames with compressed BA coming from this peer MAC
    tANI_U8 rxBApolicy;            // immediate ACK or delayed ACK for frames from this peer MAC
    tANI_U8 txCompBA;              // 1: using compressed BA to send to this peer MAC
    tANI_U8 txBApolicy;            // immediate ACK or delayed ACK for frames to this peer MAC
    tANI_U8 rxUseBA;
    tANI_U8 txUseBA;
    tANI_U8 rxBufferSize;
    tANI_U8 txBufferSize;
    tANI_U16 txBAWaitTimeout;
    tANI_U16 rxBAWaitTimeout;
} tTCParams;


typedef struct
{
    // First two fields bssid and assocId are used to find staid for sta.
    // BSSID of STA
    tSirMacAddr bssId;

    // ASSOC ID, as assigned by PE/LIM. This needs to be assigned
    // on a per BSS basis
    tANI_U16 assocId;

    // Field to indicate if this is sta entry for itself STA adding entry for itself
    // or remote (AP adding STA after successful association.
    // This may or may not be required in production driver.
    tANI_U8 staType;       // 0 - Self, 1 other/remote, 2 - bssid

    tANI_U8 shortPreambleSupported;

    // MAC Address of STA
    tSirMacAddr staMac;

    // Listen interval.
    tANI_U16 listenInterval;

    // Support for 11e/WMM
    tANI_U8 wmmEnabled;

    //
    // U-APSD Flags: 1b per AC
    // Encoded as follows:
    // b7 b6 b5 b4 b3 b2 b1 b0
    // X  X  X  X  BE BK VI VO
    //
    tANI_U8 uAPSD;

    // Max SP Length
    tANI_U8 maxSPLen;

    // 11n HT capable STA
    tANI_U8 htCapable;

    // 11n Green Field preamble support
    // 0 - Not supported, 1 - Supported
    // Add it to RA related fields of sta entry in HAL
    tANI_U8 greenFieldCapable;

    // TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz
    tANI_U8 txChannelWidthSet;

    // MIMO Power Save
    tSirMacHTMIMOPowerSaveState mimoPS;

    // RIFS mode: 0 - NA, 1 - Allowed
    tANI_U8 rifsMode;

    // L-SIG TXOP Protection mechanism
    // 0 - No Support, 1 - Supported
    // SG - there is global field.
    tANI_U8 lsigTxopProtection;

    // delayed ba support
    tANI_U8 delBASupport;
    // delayed ba support... TBD

#ifdef ANI_DVT_DEBUG
    //These 6 fields are used only by DVT driver to pass selected
    //rates to Softmac through HAL.
    tANI_U8 primaryRateIndex, secondaryRateIndex, tertiaryRateIndex;
    tANI_U8 primaryRateIndex40, secondaryRateIndex40, tertiaryRateIndex40;
#endif

    // FIXME
    //Add these fields to message
    tANI_U8 us32MaxAmpduDuration;                //in units of 32 us.
    tANI_U8 maxAmpduSize;                        // 0 : 8k , 1 : 16k, 2 : 32k, 3 : 64k
    tANI_U8 maxAmpduDensity;                     // 3 : 0~7 : 2^(11nAMPDUdensity -4)
    tANI_U8 maxAmsduSize;                        // 1 : 3839 bytes, 0 : 7935 bytes

    // TC parameters
    tTCParams staTCParams[STACFG_MAX_TC];

    // Compression and Concat parameters for DPU
    tANI_U16 deCompEnable;
    tANI_U16 compEnable;
    tANI_U16 concatSeqRmv;
    tANI_U16 concatSeqIns;


    //11n Parameters

    /**
      HT STA should set it to 1 if it is enabled in BSS
      HT STA should set it to 0 if AP does not support it.
      This indication is sent to HAL and HAL uses this flag
      to pickup up appropriate 40Mhz rates.
      */
    tANI_U8 fDsssCckMode40Mhz;


    //short GI support for 40Mhz packets
    tANI_U8 fShortGI40Mhz;

    //short GI support for 20Mhz packets
    tANI_U8 fShortGI20Mhz;



    /*
     * All the legacy and airgo supported rates.
     * These rates are the intersection of peer and self capabilities.
     */
    tSirSupportedRates supportedRates;





    /*
     * Following parameters are for returning status and station index from HAL to PE
     * via response message. HAL does not read them.
     */
    // The return status of SIR_HAL_ADD_STA_REQ is reported here
    eHalStatus status;
    // Station index; valid only when 'status' field value is eHAL_STATUS_SUCCESS
    tANI_U8 staIdx;

    //BSSID of BSS to which the station is associated.
    //This should be filled back in by HAL, and sent back to LIM as part of
    //the response message, so LIM can cache it in the station entry of hash table.
    //When station is deleted, LIM will make use of this bssIdx to delete
    //BSS from hal tables and from softmac.
    tANI_U8 bssIdx;

    /* this requires change in testDbg. I will change it later after coordinating with Diag team.
       tANI_U8 fFwdTrigerSOSPtoHost; //trigger to start service period
       tANI_U8 fFwdTrigerEOSPtoHost; //trigger to end service period
       */

   //HAL should update the existing STA entry, if this flag is set.
   //PE will set this flag in case of reassoc, where we want to resue the
   //the old staID and still return success.
    tANI_U8 updateSta;
    //A flag to indicate to HAL if the response message is required.
    tANI_U8 respReqd;

    /* Robust Management Frame (RMF) enabled/disabled */
    tANI_U8 rmfEnabled;

    /* The unicast encryption type in the association */
    tANI_U32 encryptType;
    
    /*The DPU signatures will be sent eventually to TL to help it determine the 
      association to which a packet belongs to*/
    /*Unicast DPU index*/
    tANI_U8     ucUcastSig;

    /*Broadcast DPU index*/
    tANI_U8     ucBcastSig;

    tANI_U8     sessionId; //PE session id for PE<->HAL interface 
    // HAL just sends back what it receives.

    /*if this is a P2P Capable Sta*/
    tANI_U8     p2pCapableSta;
    tANI_U32    currentOperChan;
#ifdef WLAN_FEATURE_11AC
    tANI_U8    vhtCapable;
    tANI_U8    vhtTxChannelWidthSet;
    tANI_U8    vhtTxBFCapable;
    tANI_U8    vhtTxMUBformeeCapable;
#endif

    tANI_U8    htLdpcCapable;
    tANI_U8    vhtLdpcCapable;
} tAddStaParams, *tpAddStaParams;


typedef struct
{
    // Station index
    tANI_U16 staIdx;
    tANI_U16 templIdx;
    tANI_U8   rateIdx;

    // The return status of SIR_HAL_UPDATE_STARATEINFO_REQ is reported here
    eHalStatus status;

    //A flag to indicate to HAL if the response message is required.
    tANI_U8 respReqd;

} tUpdateTxCmdTemplParams, *tpUpdateTxCmdTemplParams;
//FIXME: change the structure name








typedef struct
{
    // index of STA to delete - this should be the same as the index returned
    // as part of the AddSta
    tANI_U16 staIdx;
    tANI_U16 assocId;
    eHalStatus  status;    // Status of SIR_HAL_DELETE_STA_REQ is reported here
    tANI_U8 respReqd;
    tANI_U8     sessionId; // PE session id for PE<->HAL interface 
    // PE session id now added to all HAL<->PE transacations
    // HAL sends it back unmodified.
} tDeleteStaParams, * tpDeleteStaParams;

/*
 * This is used by PE to configure the key information on a given station.
 * When the secType is WEP40 or WEP104, the defWEPIdx is used to locate
 * a preconfigured key from a BSS the station assoicated with; otherwise
 * a new key descriptor is created based on the key field.
 */
typedef struct
{
    tANI_U16        staIdx;
    tAniEdType      encType;        // Encryption/Decryption type
    tAniWepType     wepType;        // valid only for WEP
    tANI_U8         defWEPIdx;      // Default WEP key, valid only for static WEP, must between 0 and 3
    tSirKeys        key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];            // valid only for non-static WEP encyrptions
    tANI_U8         singleTidRc;    // 1=Single TID based Replay Count, 0=Per TID based RC
    /*
     * Following parameter is for returning status
     * via response message. HAL does not read them.
     */
    eHalStatus  status;    // status of SIR_HAL_SET_STAKEY_REQ is reported here
    tANI_U8     sessionId; // PE session id for PE<->HAL interface 

    // PE session id now added to all HAL<->PE transacations
    // HAL sends back response with no modification
} tSetStaKeyParams, *tpSetStaKeyParams;

//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_ADD_BSS_REQ
//
typedef struct
{
    // MAC Address/BSSID
    tSirMacAddr bssId;
#ifdef HAL_SELF_STA_PER_BSS
    // Self Mac Address
    tSirMacAddr  selfMacAddr;
#endif
    // BSS type
    // FIXME - Is this reqd? Do we want to isolate BSS/IBSS parameters?
    tSirBssType bssType;

    // AP - 0; STA - 1 ;
    tANI_U8 operMode;

    // Network type - b/g/a/MixedMode/GreenField/Legacy
    // TODO - This enum to be updated for HT support
    // Review FIXME - Why is this needed?
    tSirNwType nwType;

    tANI_U8 shortSlotTimeSupported;
    tANI_U8 llaCoexist;
    tANI_U8 llbCoexist;
    tANI_U8 llgCoexist;
    tANI_U8 ht20Coexist;
    tANI_U8 llnNonGFCoexist;
    tANI_U8 fLsigTXOPProtectionFullSupport;
    tANI_U8 fRIFSMode;

    // Beacon Interval
    tSirMacBeaconInterval beaconInterval;

    // DTIM period
    tANI_U8 dtimPeriod;

    // CF Param Set
    // Review FIXME - Does HAL need this?
    tSirMacCfParamSet cfParamSet;

    // MAC Rate Set
    // Review FIXME - Does HAL need this?
    tSirMacRateSet rateSet;

    // 802.11n related HT parameters that are dynamic

    // Enable/Disable HT capabilities
    tANI_U8 htCapable;

    // Enable/Disable OBSS protection
    tANI_U8 obssProtEnabled;

    // RMF enabled/disabled
    tANI_U8 rmfEnabled;

    // HT Operating Mode
    // Review FIXME - Does HAL need this?
    tSirMacHTOperatingMode htOperMode;

    // Dual CTS Protection: 0 - Unused, 1 - Used
    tANI_U8 dualCTSProtection;

    // TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz
    tANI_U8 txChannelWidthSet;

    // Current Operating Channel
    tANI_U8 currentOperChannel;

    // Current Extension Channel, if applicable
    tANI_U8 currentExtChannel;

    // Add a STA entry for "itself" -
    // On AP  - Add the AP itself in an "STA context"
    // On STA - Add the AP to which this STA is joining in an "STA context"
    tAddStaParams staContext;

    /*
    * Following parameters are for returning status and station index from HAL to PE
    * via response message. HAL does not read them.
    */
    // The return status of SIR_HAL_ADD_BSS_REQ is reported here
    eHalStatus status;
    // BSS index allocated by HAL.
    // valid only when 'status' field is eHAL_STATUS_SUCCESS
    tANI_U16 bssIdx;

    // Broadcast DPU descriptor index allocated by HAL and used for broadcast/multicast packets.
    // valid only when 'status' field is eHAL_STATUS_SUCCESS
    tANI_U8    bcastDpuDescIndx;

    // DPU signature to be used for broadcast/multicast packets
    // valid only when 'status' field is eHAL_STATUS_SUCCESS
    tANI_U8    bcastDpuSignature;

    // DPU descriptor index allocated by HAL, used for bcast/mcast management packets
    tANI_U8    mgmtDpuDescIndx;

    // DPU signature to be used for bcast/mcast management packets
    tANI_U8    mgmtDpuSignature;

   //HAL should update the existing BSS entry, if this flag is set.
   //PE will set this flag in case of reassoc, where we want to resue the
   //the old bssID and still return success.
    tANI_U8 updateBss;

    // Add BSSID info for rxp filter in IBSS mode
    tSirMacSSid ssId;

    //HAL will send the response message to LIM only when this flag is set.
    //LIM will set this flag, whereas DVT will not set this flag.
    tANI_U8 respReqd;
    tANI_U8     sessionId; // PE session id for PE<->HAL interface 
    // PE session id now added to all HAL<->PE transacations
    // HAL Sends the sessionId unmodified.

#if defined WLAN_FEATURE_VOWIFI
    tPowerdBm txMgmtPower; //HAL fills in the tx power used for mgmt frames in this field.
    tPowerdBm maxTxPower;  //max power to be used after applying the power constraint, if any
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
    tANI_U8 extSetStaKeyParamValid; //Ext Bss Config Msg if set
    tSetStaKeyParams extSetStaKeyParam;  //SetStaKeyParams for ext bss msg
#endif

    tANI_U8   ucMaxProbeRespRetryLimit;  //probe Response Max retries
    tANI_U8   bHiddenSSIDEn;             //To Enable Hidden ssid.      
    tANI_U8   bProxyProbeRespEn;         //To Enable Disable FW Proxy Probe Resp
    tANI_U8   halPersona;         //Persona for the BSS can be STA,AP,GO,CLIENT value same as tVOS_CON_MODE

    //Spectrum Management Capability, 1 - Enabled, 0 - Disabled.
    tANI_U8 bSpectrumMgtEnabled;
#ifdef WLAN_FEATURE_11AC
    tANI_U8 vhtCapable;
    tANI_U8    vhtTxChannelWidthSet;
#endif
} tAddBssParams, * tpAddBssParams;

typedef struct
{
    tANI_U8 bssIdx;
    // The return status of SIR_HAL_DELETE_BSS_REQ is reported here
    eHalStatus status;
    //HAL will send the response message to LIM only when this flag is set.
    //LIM will set this flag, whereas DVT will not set this flag.
    tANI_U8 respReqd;
    tANI_U8     sessionId; // PE session id for PE<->HAL interface 
                           // HAL sends it back unmodified.
    tSirMacAddr bssid; // Will be removed for PE-HAL integration
} tDeleteBssParams, * tpDeleteBssParams;

//
// UAPSD AC mask: 1b per AC
// LSB 4 bits for delivery enabled setting. msb 4 bits for trigger enabled settings. 
// Encoded as follows:
// b7 b6 b5 b4 b3 b2 b1 b0
// BE  BK  VI  VO  BE BK VI VO

typedef struct
{
    tANI_U8 staIdx;
    tANI_U8 uapsdACMask; 
    tANI_U8 maxSpLen;    
} tUpdateUapsdParams, * tpUpdateUapsdParams;

typedef struct sSirScanEntry
{
    tANI_U8 bssIdx[HAL_NUM_BSSID];
    tANI_U8 activeBSScnt;
}tSirScanEntry, *ptSirScanEntry;

//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_INIT_SCAN_REQ
//
typedef struct {

    eHalSysMode scanMode;

    tSirMacAddr bssid;

    tANI_U8 notifyBss;

    tANI_U8 useNoA;

    // If this flag is set HAL notifies PE when SMAC returns status.
    tANI_U8 notifyHost;

    tANI_U8 frameLength;
    tANI_U8 frameType;     // Data NULL or CTS to self

    // Indicates the scan duration (in ms)
    tANI_U16 scanDuration;

    // For creation of CTS-to-Self and Data-NULL MAC packets
    tSirMacMgmtHdr macMgmtHdr;

    tSirScanEntry scanEntry;

    // when this flag is set, HAL should check for link traffic prior to scan
    tSirLinkTrafficCheck    checkLinkTraffic;

    /*
     * Following parameters are for returning status and station index from HAL to PE
     * via response message. HAL does not read them.
     */
    // The return status of SIR_HAL_INIT_SCAN_REQ is reported here
    eHalStatus status;

} tInitScanParams, * tpInitScanParams;

typedef enum  eDelStaReasonCode{
   HAL_DEL_STA_REASON_CODE_KEEP_ALIVE = 0x1,
   HAL_DEL_STA_REASON_CODE_TIM_BASED  = 0x2,
   HAL_DEL_STA_REASON_CODE_RA_BASED   = 0x3,
   HAL_DEL_STA_REASON_CODE_UNKNOWN_A2 = 0x4
}tDelStaReasonCode;

//
// Msg header is used from tSirMsgQ
// Msg Type = SIR_LIM_DELETE_STA_CONTEXT_IND
//
typedef struct {
    tANI_U16    assocId;
    tANI_U16    staId;
    tSirMacAddr bssId; // TO SUPPORT BT-AMP    
                       // HAL copies bssid from the sta table.
    tSirMacAddr addr2;        //  
    tANI_U16    reasonCode;   // To unify the keepalive / unknown A2 / tim-based disa                                                                                                 
} tDeleteStaContext, * tpDeleteStaContext;


//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_START_SCAN_REQ
// FIXME - Can we just use tSirMsgQ directly, instead of using this structure?
//
typedef struct {

    // Indicates the current scan channel
    tANI_U8 scanChannel;

    /*
    * Following parameters are for returning status and station index from HAL to PE
    * via response message. HAL does not read them.
    */
    // The return status of SIR_HAL_START_SCAN_REQ is reported here
    eHalStatus status;

#if defined WLAN_FEATURE_VOWIFI
    tANI_U32 startTSF[2];
    tPowerdBm txMgmtPower; //HAL fills in the tx power used for mgmt frames in this field.
#endif
} tStartScanParams, * tpStartScanParams;

//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_END_SCAN_REQ
// FIXME - Can we just use tSirMsgQ directly, instead of using this structure?
//
typedef struct {

    // Indicates the current scan channel
    tANI_U8 scanChannel;

    /*
    * Following parameters are for returning status and station index from HAL to PE
    * via response message. HAL does not read them.
    */
    // The return status of SIR_HAL_END_SCAN_REQ is reported here
    eHalStatus status;

} tEndScanParams, * tpEndScanParams;

//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_FINISH_SCAN_REQ
//
typedef struct {

    // Identifies the operational state of the AP/STA.
    // In case of the STA, only if the operState is non-zero will the rest of
    // the parameters that follow be decoded
    // In case of the AP, all parameters are valid
    //
    // 0 - Idle state, 1 - Link Established

    eHalSysMode scanMode;

    tSirMacAddr bssid;

    // Current operating channel
    tANI_U8 currentOperChannel;

    // If 20/40 MHz is operational, this will indicate the 40 MHz extension
    // channel in combination with the control channel
    ePhyChanBondState cbState;

    // For an STA, indicates if a Data NULL frame needs to be sent
    // to the AP with FrameControl.PwrMgmt bit set to 0
    tANI_U8 notifyBss;

    tANI_U8 notifyHost;

    tANI_U8 frameLength;
    tANI_U8 frameType;     // Data NULL or CTS to self

    // For creation of CTS-to-Self and Data-NULL MAC packets
    tSirMacMgmtHdr macMgmtHdr;

    tSirScanEntry scanEntry;

    /*
    * Following parameters are for returning status and station index from HAL to PE
    * via response message. HAL does not read them.
    */
    // The return status of SIR_HAL_FINISH_SCAN_REQ is reported here
    eHalStatus status;

} tFinishScanParams, * tpFinishScanParams;

#ifdef FEATURE_OEM_DATA_SUPPORT 

#ifndef OEM_DATA_REQ_SIZE
#define OEM_DATA_REQ_SIZE 134
#endif
#ifndef OEM_DATA_RSP_SIZE
#define OEM_DATA_RSP_SIZE 1968
#endif

typedef struct
{
    tSirMacAddr          selfMacAddr;
    eHalStatus           status;
    tANI_U8              oemDataReq[OEM_DATA_REQ_SIZE];
} tStartOemDataReq, *tpStartOemDataReq;

typedef struct 
{
    tANI_U8             oemDataRsp[OEM_DATA_RSP_SIZE];
} tStartOemDataRsp, *tpStartOemDataRsp;
#endif

typedef struct sBeaconGenStaInfo {
    tANI_U16    assocId;
    tANI_U32    staTxAckCnt;
}tBeaconGenStaInfo, *tpBeaconGenStaInfo;
//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_LIM_BEACON_GEN_IND
//

typedef struct sBeaconGenParams {
    // Identifies the BSSID for which it is time to generate a beacon
    tANI_U8                 bssIdx;
    tSirMacAddr           bssId;
#ifdef FIXME_VOLANS
    tANI_U8                 numOfSta;                /* Number of stations in power save, who have data pending*/
    tANI_U8                 numOfStaWithoutData; /* Number of stations in power save, who don't have any data pending*/
    tANI_U8                 fBroadcastTrafficPending ;
    tANI_U8                 dtimCount;
#endif
    tANI_U8                 rsvd[3];                /** Align the Structure to 4 bytes as unalligned access will happen if
                                                    the staInfo is being Accessed */
/** NOTE:   tBeaconGenStaInfo     staInfo[xx];  Depending on the Number of STA in PS, Every time
                            this array is being allocated and piled up at the End*/
} tBeaconGenParams, * tpBeaconGenParams;

typedef struct {
    tSirMacAddr bssId;
    tANI_U8 *beacon;     // Beacon data.
    tANI_U32 beaconLength; //length of the template.
    tANI_U32 timIeOffset; //TIM IE offset from the beginning of the template.
    tANI_U16 p2pIeOffset; //P2P IE offset from the begining of the template
} tSendbeaconParams, * tpSendbeaconParams;

typedef struct sSendProbeRespParams {
    tSirMacAddr bssId;
    tANI_U8      *pProbeRespTemplate; 
    tANI_U32     probeRespTemplateLen;
    tANI_U32     ucProxyProbeReqValidIEBmap[8];
} tSendProbeRespParams, * tpSendProbeRespParams;

/*
 * This is used by PE to create a set of WEP keys for a given BSS.
 */
typedef struct
{
    tANI_U8         bssIdx;
    tAniEdType      encType;
    tANI_U8         numKeys;
    tSirKeys        key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];
    tANI_U8         singleTidRc;    // 1=Single TID based Replay Count, 0=Per TID based RC
    /*
     * Following parameter is for returning status
     * via response message. HAL does not read them.
     */
    eHalStatus  status;     // status of SIR_HAL_SET_BSSKEY_REQ is reported here
    tANI_U8     sessionId;  // PE session id for PE<->HAL interface 
                            // HAL sends this unmodified in the response
} tSetBssKeyParams, *tpSetBssKeyParams;

/*
 * This is used by PE to Remove the key information on a given station.
 */
typedef struct
{
    tANI_U16         staIdx;
    tAniEdType      encType;    // Encryption/Decryption type
    tANI_U8           keyId;
    tANI_BOOLEAN    unicast;
    /*
     * Following parameter is for returning status
     * via response message. HAL does not read them.
     */
    eHalStatus  status;     // return status of SIR_HAL_REMOVE_STAKEY_REQ 
    tANI_U8     sessionId;  // PE session id for PE<->HAL interface 
                            //  HAL Sends back the PE session 
                            //  id unmodified 
} tRemoveStaKeyParams, *tpRemoveStaKeyParams;

/*
 * This is used by PE to remove keys for a given BSS.
 */
typedef struct
{
    tANI_U8         bssIdx;
    tAniEdType     encType;
    tANI_U8         keyId;
    tANI_U8         wepType;
    /*
     * Following parameter is for returning status
     * via response message. HAL does not read them.
     */
    eHalStatus  status;    // return status of SIR_HAL_REMOVE_BSSKEY_REQ 
    tANI_U8     sessionId; // PE session id for PE<->HAL interface 
                           //  HAL Sends back the PE session 
                           //  id unmodified 
} tRemoveBssKeyParams, *tpRemoveBssKeyParams;

typedef struct
{
    // index of STA to get the statistics from
    tANI_U16 staIdx;
    tANI_U8 encMode;
    // The return status of SIR_HAL_DPU_STATS_REQ is reported here
    eHalStatus status;
    // The return statistics
    tANI_U32  sendBlocks;
    tANI_U32  recvBlocks;
    tANI_U32  replays;
    tANI_U8   micErrorCnt;
    tANI_U32  protExclCnt;
    tANI_U16  formatErrCnt;
    tANI_U16  unDecryptableCnt;
    tANI_U32  decryptErrCnt;
    tANI_U32  decryptOkCnt;

} tDpuStatsParams, * tpDpuStatsParams;


/*
 * Get the DPU signature based on a given staId
 */
typedef struct
{
    tANI_U16        staIdx;
    /*
     * Following parameter is for returning status
     * via response message. HAL does not read them.
     */
    // The return status of SIR_HAL_SET_BSSKEY_REQ is reported here
    eHalStatus status;
    tANI_U8    dpuDescIndx;
    tANI_U8    dpuSignature;
} tGetDpuParams, *tpGetDpuParams;



//HAL MSG: SIR_HAL_UPDATE_BEACON_IND
typedef struct
{

    tANI_U8  bssIdx;

    //shortPreamble mode. HAL should update all the STA rates when it
    //receives this message
    tANI_U8 fShortPreamble;
    //short Slot time.
    tANI_U8 fShortSlotTime;
    //Beacon Interval
    tANI_U16 beaconInterval;
    //Protection related
    tANI_U8 llaCoexist;
    tANI_U8 llbCoexist;
    tANI_U8 llgCoexist;
    tANI_U8 ht20MhzCoexist;
    tANI_U8 llnNonGFCoexist;
    tANI_U8 fLsigTXOPProtectionFullSupport;
    tANI_U8 fRIFSMode;

    tANI_U16 paramChangeBitmap;
}tUpdateBeaconParams, *tpUpdateBeaconParams;

typedef struct 
{
   tANI_U16   opMode;
   tANI_U16  staId;
}tUpdateVHTOpMode, *tpUpdateVHTOpMode;

//HAL MSG: SIR_HAL_UPDATE_CF_IND
typedef struct
{

    tANI_U8  bssIdx;

    /*
    * cfpCount indicates how many DTIMs (including the current frame) appear before the next CFP start.
    * A CFPCount of 0 indicates that the current DTIM marks the start of the CFP.
    */
    tANI_U8  cfpCount;

    /* cfpPeriod indicates the number of DTIM intervals between the start of CFPs. */
    tANI_U8 cfpPeriod;

}tUpdateCFParams, *tpUpdateCFParams;



//HAL MSG: SIR_HAL_UPDATE_DTIM_IND
//This message not required, as Softmac is supposed to read these values from the beacon.
//PE should not look at TIM element

/*
typedef struct
{
    tANI_U8  bssIdx;


    //The DTIM Count field indicates how many beacons (including the current frame) appear before the next
    // DTIM. A DTIM Count of 0 indicates that the current TIM is a DTIM.
    //
    tANI_U8 dtimCount;


   // The DTIM Period field indicates the number of Beacon intervals between successive DTIMs. If all TIMs are
   // DTIMs, the DTIM Period field has the value 1. The DTIM Period value 0 is reserved.
    //
    tANI_U8 dtimPeriod;

}tUpdateDtimParams, *tpUpdateDtimParams;
*/

typedef enum
{
    eHAL_CHANNEL_SWITCH_SOURCE_SCAN,
    eHAL_CHANNEL_SWITCH_SOURCE_LISTEN,
    eHAL_CHANNEL_SWITCH_SOURCE_MCC,
    eHAL_CHANNEL_SWITCH_SOURCE_CSA,
    eHAL_CHANNEL_SWITCH_SOURCE_MAX = 0x7fffffff
} eHalChanSwitchSource;


//HAL MSG: SIR_HAL_CHNL_SWITCH_REQ
typedef struct
{
    tANI_U8 channelNumber;
#ifndef WLAN_FEATURE_VOWIFI    
    tANI_U8 localPowerConstraint;
#endif /* WLAN_FEATURE_VOWIFI  */
    ePhyChanBondState secondaryChannelOffset;
    tANI_U8 peSessionId;
#if defined WLAN_FEATURE_VOWIFI
    tPowerdBm txMgmtPower; //HAL fills in the tx power used for mgmt frames in this field.
    tPowerdBm maxTxPower;
    tSirMacAddr selfStaMacAddr;
                        //the request has power constraints, this should be applied only to that session
#endif
    eHalChanSwitchSource channelSwitchSrc;

    /* VO Wifi comment: BSSID is needed to identify which session issued this request. As the 
       request has power constraints, this should be applied only to that session */
    /* V IMP: Keep bssId field at the end of this msg. It is used to mantain backward compatbility
     * by way of ignoring if using new host/old FW or old host/new FW since it is at the end of this struct
     */
    tSirMacAddr bssId;

    eHalStatus status;

}tSwitchChannelParams, *tpSwitchChannelParams;

typedef void (*tpSetLinkStateCallback)(tpAniSirGlobal pMac, void *msgParam );

typedef struct sLinkStateParams
{
    // SIR_HAL_SET_LINK_STATE
    tSirMacAddr bssid;
    tSirMacAddr selfMacAddr;
    tSirLinkState state;
    tpSetLinkStateCallback callback;
    void *callbackArg;
#ifdef WLAN_FEATURE_VOWIFI_11R
    int ft;
    void * session;
#endif
} tLinkStateParams, * tpLinkStateParams;


typedef struct
{
  tANI_U16 staIdx;
  tANI_U16 tspecIdx; //TSPEC handler uniquely identifying a TSPEC for a STA in a BSS
  tSirMacTspecIE   tspec;
  eHalStatus       status;
  tANI_U8          sessionId;          //PE session id for PE<->HAL interface 
} tAddTsParams, *tpAddTsParams;

typedef struct
{
  tANI_U16 staIdx;
  tANI_U16 tspecIdx; //TSPEC identifier uniquely identifying a TSPEC for a STA in a BSS
  tSirMacAddr bssId; //TO SUPPORT BT-AMP
  
} tDelTsParams, *tpDelTsParams;

#ifdef WLAN_FEATURE_VOWIFI_11R

#define HAL_QOS_NUM_TSPEC_MAX 2
#define HAL_QOS_NUM_AC_MAX 4

typedef struct
{
  tANI_U16 staIdx;
  tANI_U16 tspecIdx; //TSPEC handler uniquely identifying a TSPEC for a STA in a BSS
  tSirMacTspecIE   tspec[HAL_QOS_NUM_AC_MAX];
  eHalStatus       status[HAL_QOS_NUM_AC_MAX];
  tANI_U8          sessionId;          //PE session id for PE<->HAL interface 
}tAggrAddTsParams, *tpAggrAddTsParams;

#endif /* WLAN_FEATURE_VOWIFI_11R */


typedef tSirRetStatus (*tHalMsgCallback)(tpAniSirGlobal pMac, tANI_U32 mesgId, void *mesgParam );


typedef struct
{
  tANI_U16 bssIdx;
  tANI_BOOLEAN highPerformance;
  tSirMacEdcaParamRecord acbe; // best effort
  tSirMacEdcaParamRecord acbk; // background
  tSirMacEdcaParamRecord acvi; // video
  tSirMacEdcaParamRecord acvo; // voice
} tEdcaParams, *tpEdcaParams;

/*
* Function Prototypes
*/

eHalStatus halMsg_setPromiscMode(tpAniSirGlobal pMac);


//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_ADDBA_REQ
//
typedef struct sAddBAParams
{

    // Station Index
    tANI_U16 staIdx;

    // Peer MAC Address
    tSirMacAddr peerMacAddr;

    // ADDBA Action Frame dialog token
    // HAL will not interpret this object
    tANI_U8 baDialogToken;

    // TID for which the BA is being setup
    // This identifies the TC or TS of interest
    tANI_U8 baTID;

    // 0 - Delayed BA (Not supported)
    // 1 - Immediate BA
    tANI_U8 baPolicy;

    // Indicates the number of buffers for this TID (baTID)
    // NOTE - This is the requested buffer size. When this
    // is processed by HAL and subsequently by HDD, it is
    // possible that HDD may change this buffer size. Any
    // change in the buffer size should be noted by PE and
    // advertized appropriately in the ADDBA response
    tANI_U16 baBufferSize;

    // BA timeout in TU's
    // 0 means no timeout will occur
    tANI_U16 baTimeout;

    // b0..b3 - Fragment Number - Always set to 0
    // b4..b15 - Starting Sequence Number of first MSDU
    // for which this BA is setup
    tANI_U16 baSSN;

    // ADDBA direction
    // 1 - Originator
    // 0 - Recipient
    tANI_U8 baDirection;

    //
    // Following parameters are for returning status from
    // HAL to PE via response message. HAL does not read them
    //
    // The return status of SIR_HAL_ADDBA_REQ is reported
    // in the SIR_HAL_ADDBA_RSP message
    eHalStatus status;

    // Indicating to HAL whether a response message is required.
    tANI_U8 respReqd;
    tANI_U8    sessionId; // PE session id for PE<->HAL interface 
                          //  HAL Sends back the PE session 
                          //  id unmodified 

} tAddBAParams, * tpAddBAParams;


//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_DELBA_IND
//
typedef struct sDelBAParams
{

    // Station Index
    tANI_U16 staIdx;

    // TID for which the BA session is being deleted
    tANI_U8 baTID;

    // DELBA direction
    // 1 - Originator
    // 0 - Recipient
    tANI_U8 baDirection;

    // FIXME - Do we need a response for this?
    // Maybe just the IND/REQ will suffice?
    //
    // Following parameters are for returning status from
    // HAL to PE via response message. HAL does not read them
    //
    // The return status of SIR_HAL_DELBA_REQ is reported
    // in the SIR_HAL_DELBA_RSP message
    //eHalStatus status;

} tDelBAParams, * tpDelBAParams;


//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_SET_MIMOPS_REQ
//
typedef struct sSet_MIMOPS
{
    // Station Index
    tANI_U16 staIdx;

    // MIMO Power Save State
    tSirMacHTMIMOPowerSaveState htMIMOPSState;
    // The return status of SIR_HAL_SET_MIMOPS_REQ is reported
    // in the SIR_HAL_SET_MIMOPS_RSP message
    eHalStatus status;
    tANI_U8     fsendRsp;

} tSetMIMOPS, * tpSetMIMOPS;


//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_EXIT_BMPS_REQ
//
typedef struct sExitBmpsParams
{
    tANI_U8     sendDataNull;
    eHalStatus  status;
    tANI_U8     bssIdx;
} tExitBmpsParams, *tpExitBmpsParams;

//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_ENTER_UAPSD_REQ
//
typedef struct sUapsdParams
{
    tANI_U8     bkDeliveryEnabled:1;
    tANI_U8     beDeliveryEnabled:1;
    tANI_U8     viDeliveryEnabled:1;
    tANI_U8     voDeliveryEnabled:1;
    tANI_U8     bkTriggerEnabled:1;
    tANI_U8     beTriggerEnabled:1;
    tANI_U8     viTriggerEnabled:1;
    tANI_U8     voTriggerEnabled:1;
    eHalStatus  status;
    tANI_U8     bssIdx;
}tUapsdParams, *tpUapsdParams;

//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_EXIT_UAPSD_REQ
//
typedef struct sExitUapsdParams
{
    eHalStatus  status;
    tANI_U8     bssIdx;
}tExitUapsdParams, *tpExitUapsdParams;

//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_LIM_DEL_BA_IND
//
typedef struct sBADeleteParams
{

    // Station Index
    tANI_U16 staIdx;

    // Peer MAC Address, whose BA session has timed out
    tSirMacAddr peerMacAddr;

    // TID for which a BA session timeout is being triggered
    tANI_U8 baTID;

    // DELBA direction
    // 1 - Originator
    // 0 - Recipient
    tANI_U8 baDirection;

    tANI_U32 reasonCode;

    tSirMacAddr  bssId; // TO SUPPORT BT-AMP    
                        // HAL copies the sta bssid to this.
} tBADeleteParams, * tpBADeleteParams;


// Mesg Type = SIR_LIM_ADD_BA_IND
typedef struct sBaActivityInd
{
    tANI_U16 baCandidateCnt;
    //baCandidateCnt is followed by BA Candidate List ( tAddBaCandidate)

    tSirMacAddr  bssId; // TO SUPPORT BT-AMP    
} tBaActivityInd, * tpBaActivityInd;


// Mesg Type = SIR_LIM_IBSS_PEER_INACTIVITY_IND
typedef struct sIbssPeerInactivityInd
{
   tANI_U8     bssIdx;
   tANI_U8     staIdx;
   tSirMacAddr staAddr;
}tIbssPeerInactivityInd, *tpIbssPeerInactivityInd;


typedef struct tHalIndCB
{

    tHalMsgCallback pHalIndCB;

}tHalIndCB,*tpHalIndCB;

/** Max number of bytes required for stations bitmap aligned at 4 bytes boundary */
#define HALMSG_NUMBYTES_STATION_BITMAP(x) (((x / 32) + ((x % 32)?1:0)) * 4)

typedef struct sControlTxParams
{
    tANI_BOOLEAN stopTx;
    /* Master flag to stop or resume all transmission, Once this flag is set,
     * softmac doesnt look for any other details.
     */
    tANI_U8 fCtrlGlobal;
    /* If this flag is set, staBitmap[] is valid */
    tANI_U8 ctrlSta;
    /* If this flag is set, bssBitmap and beaconBitmap is valid */
    tANI_U8 ctrlBss;

    /* When ctrlBss is set, this bitmap contains bitmap of BSS indices to be
     * stopped for resumed for transmission.
     * This is 32 bit bitmap, not array of bytes.
     */
    tANI_U32 bssBitmap;
    /* When ctrlBss is set, this bitmap contains bitmap of BSS indices to be
     * stopped for resumed for beacon transmission.
     */
    tANI_U32 beaconBitmap;

    /**
     *  Memory for the station bitmap will be allocated later based on
     *  the number of station supported.
     */
} tTxControlParams, * tpTxControlParams;

typedef struct sEnterBmpsParams
{
    //TBTT value derived from the last beacon
    tANI_U8         bssIdx;
    tANI_U64 tbtt;
    tANI_U8 dtimCount;
    //DTIM period given to HAL during association may not be valid,
    //if association is based on ProbeRsp instead of beacon.
    tANI_U8 dtimPeriod;

    // For ESE and 11R Roaming
    tANI_U8  bRssiFilterEnable;
    tANI_U32 rssiFilterPeriod;
    tANI_U32 numBeaconPerRssiAverage;

    eHalStatus status;
    tANI_U8 respReqd;
}tEnterBmpsParams, *tpEnterBmpsParams;

//BMPS response
typedef struct sEnterBmpsRspParams
{
    /* success or failure */
    tANI_U32   status;
    tANI_U8    bssIdx;
}tEnterBmpsRspParams, *tpEnterBmpsRspParams;
//
// Mesg header is used from tSirMsgQ
// Mesg Type = SIR_HAL_SET_MAX_TX_POWER_REQ
//
typedef struct sMaxTxPowerParams
{
    tSirMacAddr bssId;  // BSSID is needed to identify which session issued this request. As 
                        //the request has power constraints, this should be applied only to that session
    tSirMacAddr selfStaMacAddr;
    //In request,
    //power == MaxTx power to be used.
    //In response,
    //power == tx power used for management frames.
    tPowerdBm  power;
}tMaxTxPowerParams, *tpMaxTxPowerParams;

typedef struct sMaxTxPowerPerBandParams
{
    eCsrBand   bandInfo;
    tPowerdBm  power;
}tMaxTxPowerPerBandParams, *tpMaxTxPowerPerBandParams;

typedef struct sAddStaSelfParams
{
   tSirMacAddr selfMacAddr;
   tVOS_CON_MODE currDeviceMode;
   tANI_U32 status;
}tAddStaSelfParams, *tpAddStaSelfParams;

typedef struct sAbortScanParams
{
   tANI_U8 SessionId;
}tAbortScanParams, *tpAbortScanParams;

typedef struct sDelStaSelfParams
{
   tSirMacAddr selfMacAddr;

   tANI_U32 status;
}tDelStaSelfParams, *tpDelStaSelfParams;

typedef struct
{
    tSirMacAddr macAddr;
} tSpoofMacAddrReqParams, *tpSpoofMacAddrReqParams;

typedef struct sP2pPsParams
{
   tANI_U8   opp_ps;
   tANI_U32  ctWindow;
   tANI_U8   count; 
   tANI_U32  duration;
   tANI_U32  interval;
   tANI_U32  single_noa_duration;
   tANI_U8   psSelection;
}tP2pPsParams, *tpP2pPsParams;

#define HAL_MAX_SUPP_CHANNELS 128
#define HAL_MAX_SUPP_OPER_CLASSES 32

typedef struct sTdlsLinkEstablishParams
{
   tANI_U16  staIdx;
   tANI_U8   isResponder;
   tANI_U8   uapsdQueues;
   tANI_U8   maxSp;
   tANI_U8   isBufsta;
   tANI_U8   isOffChannelSupported;
   tANI_U8   peerCurrOperClass;
   tANI_U8   selfCurrOperClass;
   tANI_U8   validChannelsLen;
   tANI_U8   validChannels[HAL_MAX_SUPP_CHANNELS];
   tANI_U8   validOperClassesLen;
   tANI_U8   validOperClasses[HAL_MAX_SUPP_OPER_CLASSES];
   tANI_U32  status;
}tTdlsLinkEstablishParams, *tpTdlsLinkEstablishParams;

// tdlsoffchan
typedef struct sTdlsChanSwitchParams
{
   tANI_U16  staIdx;
   tANI_U8   tdlsOffCh;        // Target Off Channel
   tANI_U8   tdlsOffChBwOffset;// Target Off Channel Bandwidth offset
   tANI_U8   tdlsSwMode;     // TDLS Off Channel Mode
   tANI_U8   operClass;      //Operating class corresponding to target channel
   tANI_U32  status;
}tTdlsChanSwitchParams, *tpTdlsChanSwitchParams;

static inline void halGetTxTSFtimer(tpAniSirGlobal pMac, 
                                                tSirMacTimeStamp *pTime)
{
}

extern void SysProcessMmhMsg(tpAniSirGlobal pMac, tSirMsgQ* pMsg);

/* Beacon Filtering data structures */
typedef __ani_attr_pre_packed struct sBeaconFilterMsg
{
    tANI_U16    capabilityInfo;
    tANI_U16    capabilityMask;
    tANI_U16    beaconInterval;
    tANI_U16    ieNum;
    tANI_U8     bssIdx;
    tANI_U8     reserved;
} __ani_attr_packed tBeaconFilterMsg, *tpBeaconFilterMsg;

typedef __ani_attr_pre_packed struct sEidByteInfo
{
    tANI_U8     offset;
    tANI_U8     value;
    tANI_U8     bitMask;
    tANI_U8     ref;
} __ani_attr_packed tEidByteInfo, *tpEidByteInfo;


/* The above structure would be followed by multiple of below mentioned 
structure */
typedef __ani_attr_pre_packed struct sBeaconFilterIe
{
    tANI_U8         elementId;
    tANI_U8         checkIePresence;
    tEidByteInfo    byte;
} __ani_attr_packed tBeaconFilterIe, *tpBeaconFilterIe;

typedef __ani_attr_pre_packed struct sRemBeaconFilterMsg  
{
    tANI_U8  ucIeCount;
    tANI_U8  ucRemIeId[1];
}  __ani_attr_packed tRemBeaconFilterMsg, *tpRemBeaconFilterMsg;

#endif /* _HALMSGAPI_H_ */

