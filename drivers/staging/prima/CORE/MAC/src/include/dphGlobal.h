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

/*
 *
 * Author:      Sandesh Goel

 * Date:        02/25/02

 * History:-

 * Date            Modified by    Modification Information

 * --------------------------------------------------------------------

 *

 */


#ifndef __DPH_GLOBAL_H__

#define __DPH_GLOBAL_H__


#include "limGlobal.h"

//#include "parserApi.h"

#include "sirMacProtDef.h"

#include "sirMacPropExts.h"

#include "sirApi.h"


/// Following determines whether statistics are maintained or not

#define DPH_STATS


/// traffic category not allowed

#define DPH_TID_NOTALLOWED           0xFF


/// Periodicity of invocation of rate adaptation (in ms)

#define DPH_RATE_ADAPTATION_PERIOD     20


// Rate indices

#define DPH_PHY_RATE_1_INDEX     0

#define DPH_PHY_RATE_2_INDEX     1

#define DPH_PHY_RATE_5_5_INDEX   2

#define DPH_PHY_RATE_11_INDEX    3

#define DPH_PHY_RATE_6_INDEX     4

#define DPH_PHY_RATE_9_INDEX     5

#define DPH_PHY_RATE_12_INDEX    6

#define DPH_PHY_RATE_18_INDEX    7

#define DPH_PHY_RATE_24_INDEX    8

#define DPH_PHY_RATE_36_INDEX    9

#define DPH_PHY_RATE_48_INDEX   10

#define DPH_PHY_RATE_54_INDEX   11

#define DPH_PHY_RATE_72_INDEX   12

#define DPH_PHY_RATE_96_INDEX   13

#define DPH_PHY_RATE_108_INDEX  14

#define DPH_PHY_RATE_144_INDEX  15

#define DPH_PHY_RATE_MAX_INDEX  16


/// Maximum time to wait for a management packet to go out (ms)

#define DPH_MAX_MGMT_WAIT_TIME  10000


/// Step size for the wait time histogram (ms)

#define DPH_WAIT_HIST_STEP 20


/// Number of entries in wait time histogram

#define DPH_WAIT_HIST_SIZE  100


/// TCID for Management & Keep Alive Mgmt frames

#define DPH_MGMT_TCID                      4

#define DPH_KEEPALIVE_PROBE_RESPONSE_TCID  0


/// STAID for Management frames

#define DPH_USE_MGMT_STAID  -1


// Keep Alive frames

#define DPH_NON_KEEPALIVE_FRAME  0

#define DPH_KEEPALIVE_FRAME      1


/// Mask for subtype, type, protocol version, order and wep fields in the mac frame control

#define DPH_FC_BD_FILL_MASK  0xFFCC


/// Enable/Disable Txop generation in TFP for HCF mode

#define DPH_ENABLE_HCF_TXOP_GEN_AT_TFP   0x00

#define DPH_DISABLE_HCF_TXOP_GEN_AT_TFP  0x02


/// Enable/Disable Txop generation in TFP for EDCF mode

#define DPH_ENABLE_EDCF_TXOP_GEN_AT_TFP   0x00

#define DPH_DISABLE_EDCF_TXOP_GEN_AT_TFP  0x01


#define DPH_DUMP_ALL_STA_ID     -1

#define DPH_DUMP_RX_BD           0

#define DPH_DUMP_TX_BD           1

#define DPH_DUMP_TX_MGMT_BD      2


//DPH Hash Index for BSS(STA's Peer) on station.

#define DPH_STA_HASH_INDEX_PEER   1


#ifdef WLAN_FEATURE_11W
//DPH PMF SA Query state for station

#define DPH_SA_QUERY_NOT_IN_PROGRESS      1

#define DPH_SA_QUERY_IN_PROGRESS          2

#define DPH_SA_QUERY_TIMED_OUT            3
#endif


typedef struct sDphRateBasedCtr

{

    tANI_U32 hi;

    tANI_U32 lo;

} tDphRateBasedCtr;


typedef struct sDphPhyRates

{

    tANI_U8 dataRateX2;

    tANI_U8 ackRateX2;

    tANI_U8 rtsRateX2;

} tDphPhyRates;


typedef struct sDphIFSValues

{

    tANI_U8 sifs;

    tANI_U8 pifs;

    tANI_U8 difs;

    tANI_U8 preamble;

} tDphIFSValues;


typedef struct sDphQosParams

{

    tANI_U8                   addtsPresent;

    tSirAddtsReqInfo       addts;

    tSirMacQosCapabilityStaIE capability;

} tDphQosParams;


/// Queue attribute structure

typedef struct sDphQueueAttr

{

    tANI_U16     valid : 1;

    tANI_U16     seqNum : 12;

    tANI_U16     ackPolicy : 2;

    tANI_U16     rsvd : 1;

} tDphQueueAttr, *tpDphQueueAttr;



typedef struct sCfgTrafficClass {

    //Use Block ACK on this STA/TID

    // Fields used to store the default TC parameters for this TSPEC.

    // They will be used when the TSPEC is deleted.

    tANI_U8 fDisableTx:1;

    tANI_U8 fDisableRx:1;

    tANI_U8 fUseBATx:1;

    tANI_U8 fUseBARx:1;


    // 1: expect to see frames with compressed BA coming from this peer MAC

    tANI_U8 fRxCompBA:1;

    tANI_U8 fTxCompBA:1;


    // immediate ACK or delayed ACK for frames from this peer MAC

    tANI_U8 fRxBApolicy:1;


    // immediate ACK or delayed ACK for frames to this peer MAC

    tANI_U8 fTxBApolicy:1;


    //Initiator or recipient

    tANI_U8 role;


    //Max # of MSDU received from this STA, negotiated at ADDBA

    // used for maintaining block ack state info

    tANI_U16 rxBufSize;


    //Max # of MSDU send to this STA, negotiated at ADDBA

    tANI_U16 txBufSize;


    //BA timeout negotiated at ADDBA. Unit: TU

    tANI_U16 tuTxBAWaitTimeout; //Time for Tx to wait for BA. 0 means no timeout


    tANI_U16 tuRxBAWaitTimeout; //Time for Rx to wait for explicit/implicit BAR. 0 means no timeout


} tCfgTrafficClass;



/// STA state node

typedef struct sDphHashNode

{


    //BYTE 0

    // HASH ENTRY FIELDS NOT NEEDED IN HAL.

    /// This STA valid or not

    tANI_U8   valid : 1;

    tANI_U8   encPolicy : 3;

    tANI_U8   defaultKey : 1;

    tANI_U8   defaultKeyId : 2;

    tANI_U8   qosMode : 1;


    //BYTE 1

    tANI_U8   erpEnabled : 1;

    tANI_U8   added : 1; // This has been added to the dph hash table

    tANI_U8   linkTestOn : 1;

    tANI_U8   shortPreambleEnabled : 1;

    tANI_U8   shortSlotTimeEnabled : 1;

    tANI_U8   stopTx:1;

    tANI_U8   wmeEnabled: 1; // set if both ap and sta are wme capable

    tANI_U8   lleEnabled: 1; // set if both ap and sta are 11e capable


    //BYTE 2

    tANI_U8   wsmEnabled: 1; // set if both ap and sta are wsm capable

    tANI_U8   versionPresent:1; // station gave version info

    tANI_U8   burstEnableForce:1; // allow bursting regardless of qosMode

    tANI_U8   staAuthenticated:1;    

    /// Whether the peer is ANI or not

    tANI_U8  aniPeer:1;

    tANI_U8   titanPeer:1;                // flag to indicate if its a titan peer    

    tANI_U8  fAniCount:1;

    tANI_U8   rmfEnabled:1;


    /// Fragmentation size

    tANI_U16   fragSize;


    /// LIM state

    tLimMlmStaContext mlmStaContext;


    /// Number of Tim to wait if the STA doesn't respond / fetch data

    tANI_U8  timWaitCount;


    /// Number of Successfull MPDU's being sent

    tANI_U32    curTxMpduCnt;




    /// number of consecutive TIMs sent without response

    tANI_U8  numTimSent;


    // qos parameter info

    tDphQosParams  qos;


    // station version info - valid only if versionPresent is set

    tSirMacPropVersion version;

    // station proprietary capability

    tANI_U16                propCapability;


#ifdef PLM_WDS

    tANI_U8  wdsIndex;

    tANI_U8  wdsPeerBeaconSeen;

#endif


   //Taurus capabilities

   tANI_U16 baPolicyFlag;                 //BA Policy for each TID. 


    /*

    * All the legacy and airgo supported rates.

    */

    tSirSupportedRates supportedRates;


    tANI_U8 htGreenfield:1;

    tANI_U8 htShortGI40Mhz:1;

    tANI_U8 htShortGI20Mhz:1;

    // DSSS/CCK at 40 MHz: Enabled 1 or Disabled

    tANI_U8 htDsssCckRate40MHzSupport:1;

    // L-SIG TXOP Protection used only if peer support available

    tANI_U8 htLsigTXOPProtection:1;

    // A-MPDU Density

    // 000 - No restriction

    // 001 - 1/8 usec

    // 010 - 1/4 usec

    // 011 - 1/2 usec

    // 100 - 1 usec

    // 101 - 2 usec

    // 110 - 4 usec

    // 111 - 8 usec

    //

    tANI_U8 htAMpduDensity:3;




    //Set to 0 for 3839 octets

    //Set to 1 for 7935 octets

    tANI_U8 htMaxAmsduLength;




    // MIMO Power Save

    tSirMacHTMIMOPowerSaveState htMIMOPSState;


    //


    // Maximum Rx A-MPDU factor

    tANI_U8 htMaxRxAMpduFactor:2;

    //

    // Recommended Tx Width Set

    // 0 - use 20 MHz channel (control channel)

    // 1 - use 40 Mhz channel

    //

    tANI_U8 htSupportedChannelWidthSet:1;
    tANI_U8 htSecondaryChannelOffset:2;
    tANI_U8 rsvd1:3;


    ///////////////////////////////////////////////////////////////////////

    // DPH HASH ENTRY FIELDS NEEDED IN HAL ONLY

    ///////////////////////////////////////////////////////////////////////

    tANI_U8 dpuSig:4;                       // DPU signiture

    tANI_U8 staSig:4;                       // STA signature

    tANI_U8 staType;


    tANI_U16 bssId;                          // BSSID

    tANI_U16 assocId;                       // Association ID




    //This is the real sta index generated by HAL

    tANI_U16 staIndex;

    tANI_U8    staAddr[6];

    /*The DPU signatures will be sent eventually to TL to help it determine the 

      association to which a packet belongs to*/

    /*Unicast DPU signature*/

    tANI_U8     ucUcastSig;


    /*Broadcast DPU signature*/

    tANI_U8     ucBcastSig;


    //

    // PE needs this info on a per-STA, per-TID basis

    // At any point in time, when this data is sampled,

    // it gives a measure of:

    // a) All the active bA sessions

    // b) And the BA configuration itself

    //

    tCfgTrafficClass tcCfg[STACFG_MAX_TC];


    // Block Ack state

    // This is used between PE and HAL only.

    // can be set to one of the values from the following enum

    /*typedef enum eLimBAState

    {

        eLIM_BA_STATE_IDLE, // we are not waiting for anything from HAL.

        eLIM_BA_STATE_WT_ADD_RSP, //We are waiting for Add rsponse from HAL.

        eLIM_BA_STATE_WT_DEL_RSP //  We are waiting for Del response from HAL.

    } tLimBAState; */




    //BA state bitmap 2 bits per tid

    // BA state for tid i  = (baState >> tid*2) & 0x3

    tANI_U32 baState;

#ifdef WLAN_FEATURE_11AC
    tANI_U8  vhtSupportedChannelWidthSet;
    tANI_U8  vhtBeamFormerCapable;
#endif

#ifdef WLAN_FEATURE_11W
    tANI_U8  pmfSaQueryState;
    tANI_U8  pmfSaQueryRetryCount;
    tANI_U16 pmfSaQueryCurrentTransId;
    tANI_U16 pmfSaQueryStartTransId;
    TX_TIMER pmfSaQueryTimer;
    v_TIME_t last_unprot_deauth_disassoc;
    tANI_U8 proct_deauh_disassoc_cnt;
    v_TIME_t last_assoc_received_time;
#endif

    tANI_U8 htLdpcCapable;
    tANI_U8 vhtLdpcCapable;
    /* key installed for this STA or not in the firmware */
    tANI_U8 isKeyInstalled;
    /* When a station with already an existing dph entry tries to 

     * associate again, the old dph entry will be zeroed out except 

     * for the next pointer. The next pointer must be defined at the  

     * end of the structure.

     */
    tANI_U8 isDisassocDeauthInProgress;
    struct sDphHashNode  *next;


} tDphHashNode, *tpDphHashNode;


#include "dphHashTable.h"


// -------------------------------------------------------------------


// get protection overrides from config variable

// bit0: force cts to self protection for tx to Airgo peers

// bit1: force cts to self protection for tx to non Airgo peers

#define DPH_PROT_OVERRIDE_NONANI_PEER_GET(cfgval)    ((cfgval) & 1)

#define DPH_PROT_OVERRIDE_ANI_PEER_GET(cfgval) (((cfgval) & 2) >> 1)


typedef struct sAniSirDph

{

    /// The hash table object

    dphHashTableClass dphHashTable;

} tAniSirDph, *tpAniSirDph;


#endif




