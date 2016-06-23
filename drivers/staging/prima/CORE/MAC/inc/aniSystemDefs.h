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
 *
 * This file aniSystemDefs.h contains definitions used by
 * various ANI entities
 * Author:    Chandra Modumudi
 * Date:      09/18/2002
 * History:-
 * Date       Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#ifndef __ANI_SYSTEM_DEFS_H
#define __ANI_SYSTEM_DEFS_H

#include "sirTypes.h"
#include "sirMacProtDef.h"

#define ANI_OUI  0x000AF5

/// Max WDS info length.
#define ANI_WDS_INFO_MAX_LENGTH        64


/// Max number of SSIDs to support Multiple SSID feature
#define ANI_MAX_NUM_OF_SSIDS           16

//This is to force compiler to use the maximum of an int for enum
#define SIR_MAX_ENUM_SIZE    0x7FFFFFFF

/* Max key size  including the WAPI and TKIP */
#define WLAN_MAX_KEY_RSC_LEN         16
#define WLAN_WAPI_KEY_RSC_LEN        16



#ifndef FALSE
#undef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#undef TRUE
#define TRUE    1
#endif

typedef enum eAniBool
{
    eSIR_FALSE,
    eSIR_TRUE,
    eSIR_DONOT_USE_BOOL = SIR_MAX_ENUM_SIZE
} tAniBool;

/// Authentication type enum used with peer
typedef enum eAniAuthType
{
    eSIR_OPEN_SYSTEM,
    eSIR_SHARED_KEY,
#if defined WLAN_FEATURE_VOWIFI_11R
    eSIR_FT_AUTH,
#endif
#if defined FEATURE_WLAN_ESE
    eSIR_LEAP_AUTH = 0x80,
#endif
    eSIR_AUTO_SWITCH,
    eSIR_DONOT_USE_AUTH_TYPE = SIR_MAX_ENUM_SIZE
} tAniAuthType;

/// Encryption type enum used with peer
typedef enum eAniEdType
{
    eSIR_ED_NONE,
    eSIR_ED_WEP40,
    eSIR_ED_WEP104,
    eSIR_ED_TKIP,
    eSIR_ED_CCMP,
#if defined(FEATURE_WLAN_WAPI)
    eSIR_ED_WPI,
#endif
    /*DPU HW treats encryption mode 4 plus RMF bit set in TX BD as BIP.
      Thus while setting BIP encryption mode in corresponding DPU Desc
      eSIR_ED_AES_128_CMAC should be set to eSIR_ED_CCMP*/
    eSIR_ED_AES_128_CMAC,
    eSIR_ED_NOT_IMPLEMENTED = SIR_MAX_ENUM_SIZE
} tAniEdType;


typedef enum eAniWepType
{
    eSIR_WEP_STATIC,
    eSIR_WEP_DYNAMIC,
} tAniWepType;

/// Enum to specify whether key is used
/// for TX only, RX only or both
typedef enum eAniKeyDirection
{
    eSIR_TX_ONLY,
    eSIR_RX_ONLY,
    eSIR_TX_RX,
    eSIR_TX_DEFAULT,
    eSIR_DONOT_USE_KEY_DIRECTION = SIR_MAX_ENUM_SIZE
} tAniKeyDirection;

/// Enum for rate
typedef enum eAniRate
{
    eSIR_ANY_RATE
} tAniRate;

typedef struct sAniSSID
{
    tANI_U8        length;
    tANI_U8        ssId[SIR_MAC_MAX_SSID_LENGTH];
} tAniSSID, *tpAniSSID;

typedef struct sAniApName
{
    tANI_U8        length;
    tANI_U8        name[SIR_MAC_MAX_SSID_LENGTH];
} tAniApName, *tpAniApName;

/// RSN IE information
typedef struct sSirRSNie
{
    tANI_U16       length;
    tANI_U8        rsnIEdata[SIR_MAC_MAX_IE_LENGTH+2];
} tSirRSNie, *tpSirRSNie;

/// Additional IE information :
/// This can include WSC IE, P2P IE, and/or FTIE from upper layer.
/// MAC layer transparently convey these IE info between peer STA and upper layer,
/// but never requires to parse it.
typedef struct sSirAddie
{
    tANI_U16       length;
    tANI_U8        addIEdata[SIR_MAC_MAX_IE_LENGTH+2];
} tSirAddie, *tpSirAddie;

#ifdef FEATURE_WLAN_ESE

// The CCKM IE needs to be in the
// Join and Reassoc Req.
typedef struct sSirCCKMie
{
    tANI_U16       length;
    tANI_U8        cckmIEdata[SIR_MAC_MAX_IE_LENGTH+2];
} tSirCCKMie, *tpSirCCKMie;

#endif

/// Need to refine structure definitions for Keying material,
/// QoS policy, etc

/// Definition Quality of Service
typedef struct sSirQos
{
    tANI_U16                temp1;  // Need to define later
    tANI_U16                temp2;  // Need to define later
} tSirQos, *tpSirQos;

/// Definition for Encryption Keys
typedef struct sSirKeys
{
    tANI_U8                  keyId;
    tANI_U8                  unicast;     // 0 for multicast
    tAniKeyDirection    keyDirection;
    tANI_U8                  keyRsc[WLAN_MAX_KEY_RSC_LEN];   // Usage is unknown
    tANI_U8                  paeRole;     // =1 for authenticator,
                                     // =0 for supplicant
    tANI_U16                 keyLength;
    tANI_U8                  key[SIR_MAC_MAX_KEY_LENGTH];
} tSirKeys, *tpSirKeys;

/// Definition for Keying material
typedef struct sSirKeyMaterial
{
    tANI_U16         length;    // This is the length of all
                           // data that follows
    tAniEdType  edType;    // Encryption/Decryption type
    tANI_U8          numKeys;
    tSirKeys    key[1];
} tSirKeyMaterial, *tpSirKeyMaterial;

#define SIR_CIPHER_SEQ_CTR_SIZE 6
/// Definition for MIC failure indication
typedef struct sSirMicFailureInfo
{
    tSirMacAddr            srcMacAddr; //address used to compute MIC
    tSirMacAddr            taMacAddr; //transmitter address
    tSirMacAddr            dstMacAddr;
    tAniBool               multicast;
    tANI_U8     IV1;            // first byte of IV
    tANI_U8     keyId;          // second byte of IV
    tANI_U8     TSC[SIR_CIPHER_SEQ_CTR_SIZE]; // sequence number
    tSirMacAddr            rxMacAddr; // receive address

} tSirMicFailureInfo, *tpSirMicFailureInfo;

// Boa command. Used mainly for radar info persistance
typedef struct sBoaCommand
{
    tANI_U8     length;
    tANI_U8     cmd[64];
}tBoaCommand;


typedef __ani_attr_pre_packed struct sTrafStrmMetrics
{
    tANI_U16      UplinkPktQueueDly;
    tANI_U16      UplinkPktQueueDlyHist[4];
    tANI_U32      UplinkPktTxDly;
    tANI_U16      UplinkPktLoss;
    tANI_U16      UplinkPktCount;
    tANI_U8       RoamingCount;
    tANI_U16      RoamingDly;
} __ani_attr_packed tTrafStrmMetrics, *tpTrafStrmMetrics;


typedef __ani_attr_pre_packed struct sBcnReportFields
{
    tANI_U8       ChanNum;
    tANI_U8       Spare;
    tANI_U16      MeasDuration;
    tANI_U8       PhyType;
    tANI_U8       RecvSigPower;
    tSirMacAddr   Bssid;
    tANI_U32      ParentTsf;
    tANI_U32      TargetTsf[2];
    tANI_U16      BcnInterval;
    tANI_U16      CapabilityInfo;
} __ani_attr_packed tBcnReportFields, *tpBcnReportFields;


#endif /* __ANI_SYSTEM_DEFS_H */

