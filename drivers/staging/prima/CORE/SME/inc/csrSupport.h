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

    \file csrSupport.h

    Exports and types for the Common Scan and Roaming supporting interfaces.

   ========================================================================== */
#ifndef CSR_SUPPORT_H__
#define CSR_SUPPORT_H__

#include "csrLinkList.h"
#include "csrApi.h"
#include "vos_nvitem.h"

#ifdef FEATURE_WLAN_WAPI
#define CSR_WAPI_OUI_ROW_SIZE          ( 3 )
#define CSR_WAPI_OUI_SIZE              ( 4 )
#define CSR_WAPI_VERSION_SUPPORTED     ( 1 )
#define CSR_WAPI_MAX_AUTH_SUITES       ( 2 )
#define CSR_WAPI_MAX_CYPHERS           ( 5 )
#define CSR_WAPI_MAX_UNICAST_CYPHERS   ( 5 )
#define CSR_WAPI_MAX_MULTICAST_CYPHERS ( 1 )
#endif /* FEATURE_WLAN_WAPI */

#define CSR_RSN_OUI_SIZE              ( 4 )
#define CSR_RSN_VERSION_SUPPORTED     ( 1 )
#define CSR_RSN_MAX_AUTH_SUITES       ( 4 )
#define CSR_RSN_MAX_CYPHERS           ( 5 )
#define CSR_RSN_MAX_UNICAST_CYPHERS   ( 5 )
#define CSR_RSN_MAX_MULTICAST_CYPHERS ( 1 )

#define CSR_WPA_OUI_SIZE              ( 4 )
#define CSR_WPA_VERSION_SUPPORTED     ( 1 )
#define CSR_WME_OUI_SIZE ( 4 )
#define CSR_WPA_MAX_AUTH_SUITES       ( 2 )
#define CSR_WPA_MAX_CYPHERS           ( 5 )
#define CSR_WPA_MAX_UNICAST_CYPHERS   ( 5 )
#define CSR_WPA_MAX_MULTICAST_CYPHERS ( 1 )
#define CSR_WPA_IE_MIN_SIZE           ( 6 )   // minimum size of the IE->length is the size of the Oui + Version.
#define CSR_WPA_IE_MIN_SIZE_W_MULTICAST ( HDD_WPA_IE_MIN_SIZE + HDD_WPA_OUI_SIZE )
#define CSR_WPA_IE_MIN_SIZE_W_UNICAST   ( HDD_WPA_IE_MIN_SIZE + HDD_WPA_OUI_SIZE + sizeof( pWpaIe->cUnicastCyphers ) )

#define CSR_DOT11_SUPPORTED_RATES_MAX ( 12 )
#define CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX ( 8 )

#define CSR_DOT11_MAX_NUM_SUPPORTED_11B_RATES ( 4 )
#define CSR_DOT11_MAX_NUM_SUPPORTED_11A_RATES ( 8 )
#define CSR_DOT11_BASIC_RATE_MASK ( 0x80 )

#define CSR_WME_INFO_IE_VERSION_SUPPORTED ( 1 )
#define CSR_WME_PARM_IE_VERSION_SUPPORTED ( 1 )

#define CSR_PASSIVE_SCAN_STARTING_CHANNEL ( 52)
#define CSR_PASSIVE_SCAN_ENDING_CHANNEL ( 140)

#define CSR_DOT11_MAX_11A_RATE ( 54 * 2 )
#define CSR_DOT11_MIN_11A_RATE (  6 * 2 )
#define CSR_DOT11_MAX_11B_RATE ( 11 * 2 )
#define CSR_DOT11_MIN_11B_RATE (  1 * 2 )
#define CSR_DOT11_MAX_11G_RATE ( 54 * 2 )
#define CSR_DOT11_MIN_11G_RATE (  6 * 2 )

//Define the frequency ranges that need to be passive scan, MHz
#define CSR_PASSIVE_SCAN_CAT1_LOW       5250 
#define CSR_PASSIVE_SCAN_CAT1_HIGH      5350
#define CSR_PASSIVE_SCAN_CAT2_LOW       5470 
#define CSR_PASSIVE_SCAN_CAT2_HIGH      5725
#define CSR_PASSIVE_SCAN_CAT3_LOW       5500 
#define CSR_PASSIVE_SCAN_CAT3_HIGH      5560

#define CSR_OUI_USE_GROUP_CIPHER_INDEX 0x00
#define CSR_OUI_WEP40_OR_1X_INDEX      0x01
#define CSR_OUI_TKIP_OR_PSK_INDEX      0x02
#define CSR_OUI_RESERVED_INDEX         0x03
#define CSR_OUI_AES_INDEX              0x04
#define CSR_OUI_WEP104_INDEX           0x05

#ifdef FEATURE_WLAN_WAPI
#define CSR_OUI_WAPI_RESERVED_INDEX    0x00
#define CSR_OUI_WAPI_WAI_CERT_OR_SMS4_INDEX    0x01
#define CSR_OUI_WAPI_WAI_PSK_INDEX     0x02
#endif /* FEATURE_WLAN_WAPI */


typedef enum 
{
    // 11b rates
    eCsrSuppRate_1Mbps   =   1 * 2,
    eCsrSuppRate_2Mbps   =   2 * 2,
    eCsrSuppRate_5_5Mbps =  11,      // 5.5 * 2
    eCsrSuppRate_11Mbps  =  11 * 2,

    // 11a / 11g rates
    eCsrSuppRate_6Mbps   =   6 * 2,
    eCsrSuppRate_9Mbps   =   9 * 2,
    eCsrSuppRate_12Mbps  =  12 * 2,
    eCsrSuppRate_18Mbps  =  18 * 2,
    eCsrSuppRate_24Mbps  =  24 * 2,
    eCsrSuppRate_36Mbps  =  36 * 2,
    eCsrSuppRate_48Mbps  =  48 * 2,
    eCsrSuppRate_54Mbps  =  54 * 2,

    // airgo proprietary rates
    eCsrSuppRate_10Mbps  =  10 * 2,
    eCsrSuppRate_10_5Mbps=  21,     // 10.5 * 2
    eCsrSuppRate_20Mbps  =  20 * 2,
    eCsrSuppRate_21Mbps  =  21 * 2,
    eCsrSuppRate_40Mbps  =  40 * 2,
    eCsrSuppRate_42Mbps  =  42 * 2,
    eCsrSuppRate_60Mbps  =  60 * 2,
    eCsrSuppRate_63Mbps  =  63 * 2,
    eCsrSuppRate_72Mbps  =  72 * 2,
    eCsrSuppRate_80Mbps  =  80 * 2,
    eCsrSuppRate_84Mbps  =  84 * 2,
    eCsrSuppRate_96Mbps  =  96 * 2,
    eCsrSuppRate_108Mbps = 108 * 2,
    eCsrSuppRate_120Mbps = 120 * 2,
    eCsrSuppRate_126Mbps = 126 * 2,
    eCsrSuppRate_144Mbps = 144 * 2,
    eCsrSuppRate_160Mbps = 160 * 2,
    eCsrSuppRate_168Mbps = 168 * 2,
    eCsrSuppRate_192Mbps = 192 * 2,
    eCsrSuppRate_216Mbps = 216 * 2,
    eCsrSuppRate_240Mbps = 240 * 2
}eCsrSupportedRates;

typedef enum
{
    eCsrPassiveScanNot,     //can be scanned actively on the whole 5GHz band
    eCsrPassiveScanCat1,    //always passive scan from 5250 to 5350MHz
    eCsrPassiveScanCat2,    //always passive scan from 5250 to 5350MHz, and from 5470 to 5725MHz
    eCsrPassiveScanCat3,    //always passive scan from 5250 to 5350MHz, from 5470 to 5725MHz, and from 5500 to 5560MHz
}eCsrPassiveScanCat;


//Please donot insert in the middle of the enum here because they tie to the indiex
typedef enum
{
    eCSR_COUNTRY_INDEX_US = 0,  //Always set US as index 0
    eCSR_COUNTRY_INDEX_ANDORRA,
    eCSR_COUNTRY_INDEX_UAE,     //United Arab Emirates
    eCSR_COUNTRY_INDEX_AFGHANISTAN,
    eCSR_COUNTRY_INDEX_ANTIGUA_AND_BARBUDA,
    eCSR_COUNTRY_INDEX_ANGUILLA,
    eCSR_COUNTRY_INDEX_ALBANIA,
    eCSR_COUNTRY_INDEX_ARMENIA,
    eCSR_COUNTRY_INDEX_NETHERLANDS_ANTILLES,
    eCSR_COUNTRY_INDEX_ANGOLA,
    eCSR_COUNTRY_INDEX_ANTARCTICA,
    eCSR_COUNTRY_INDEX_ARGENTINA,
    eCSR_COUNTRY_INDEX_AMERICAN_SAMOA,
    eCSR_COUNTRY_INDEX_AUSTRIA,
    eCSR_COUNTRY_INDEX_AUSTRALIA,
    eCSR_COUNTRY_INDEX_ARUBA,
    eCSR_COUNTRY_INDEX_ALAND_ISLANDS,
    eCSR_COUNTRY_INDEX_AZERBAIJAN,
    eCSR_COUNTRY_INDEX_BOSNIA_AND_HERZEGOVINA,
    eCSR_COUNTRY_INDEX_BARBADOS,
    eCSR_COUNTRY_INDEX_BANGLADESH,
    eCSR_COUNTRY_INDEX_BELGIUM,
    eCSR_COUNTRY_INDEX_BURKINA_FASO,
    eCSR_COUNTRY_INDEX_BULGARIA,
    eCSR_COUNTRY_INDEX_BAHRAIN,
    eCSR_COUNTRY_INDEX_BURUNDI,
    eCSR_COUNTRY_INDEX_BENIN,
    eCSR_COUNTRY_INDEX_SAINT_BARTHELEMY,
    eCSR_COUNTRY_INDEX_BERMUDA,
    eCSR_COUNTRY_INDEX_BRUNEI_DARUSSALAM,
    eCSR_COUNTRY_INDEX_BOLVIA,
    eCSR_COUNTRY_INDEX_BRAZIL,
    eCSR_COUNTRY_INDEX_BAHAMAS,
    eCSR_COUNTRY_INDEX_BHUTAN,
    eCSR_COUNTRY_INDEX_BOUVET_ISLAND,
    eCSR_COUNTRY_INDEX_BOTSWANA,
    eCSR_COUNTRY_INDEX_BELARUS,
    eCSR_COUNTRY_INDEX_BELIZE,
    eCSR_COUNTRY_INDEX_CANADA,      
    eCSR_COUNTRY_INDEX_COCOS_KEELING_ISLANDS,
    eCSR_COUNTRY_INDEX_CONGO_REP,
    eCSR_COUNTRY_INDEX_CENTRAL_AFRICAN,
    eCSR_COUNTRY_INDEX_CONGO,
    eCSR_COUNTRY_INDEX_SWITZERLAND,
    eCSR_COUNTRY_INDEX_COTE_DIVOIRE,
    eCSR_COUNTRY_INDEX_COOK_ISLANDS,
    eCSR_COUNTRY_INDEX_CHILE,
    eCSR_COUNTRY_INDEX_CAMEROON,
    eCSR_COUNTRY_INDEX_CHINA,
    eCSR_COUNTRY_INDEX_COLUMBIA,
    eCSR_COUNTRY_INDEX_COSTA_RICA,
    eCSR_COUNTRY_INDEX_CUBA,
    eCSR_COUNTRY_INDEX_CAPE_VERDE,
    eCSR_COUNTRY_INDEX_CHRISTMAS_ISLAND,
    eCSR_COUNTRY_INDEX_CYPRUS,
    eCSR_COUNTRY_INDEX_CZECH,
    eCSR_COUNTRY_INDEX_GERMANY,
    eCSR_COUNTRY_INDEX_DJIBOUTI,
    eCSR_COUNTRY_INDEX_DENMARK,
    eCSR_COUNTRY_INDEX_DOMINICA,
    eCSR_COUNTRY_INDEX_DOMINICAN_REP,
    eCSR_COUNTRY_INDEX_ALGERIA,
    eCSR_COUNTRY_INDEX_ECUADOR,
    eCSR_COUNTRY_INDEX_ESTONIA,
    eCSR_COUNTRY_INDEX_EGYPT,
    eCSR_COUNTRY_INDEX_WESTERN_SAHARA,
    eCSR_COUNTRY_INDEX_ERITREA,
    eCSR_COUNTRY_INDEX_SPAIN,
    eCSR_COUNTRY_INDEX_ETHIOPIA,
    eCSR_COUNTRY_INDEX_FINLAND,
    eCSR_COUNTRY_INDEX_FIJI,
    eCSR_COUNTRY_INDEX_FALKLAND_ISLANDS,
    eCSR_COUNTRY_INDEX_MICRONESIA,
    eCSR_COUNTRY_INDEX_FAROE_ISLANDS,
    eCSR_COUNTRY_INDEX_FRANCE,
    eCSR_COUNTRY_INDEX_GABON,
    eCSR_COUNTRY_INDEX_UNITED_KINGDOM,
    eCSR_COUNTRY_INDEX_GRENADA,
    eCSR_COUNTRY_INDEX_GEORGIA,
    eCSR_COUNTRY_INDEX_FRENCH_GUIANA,
    eCSR_COUNTRY_INDEX_GUERNSEY,
    eCSR_COUNTRY_INDEX_GHANA,
    eCSR_COUNTRY_INDEX_GIBRALTAR,
    eCSR_COUNTRY_INDEX_GREENLAND,
    eCSR_COUNTRY_INDEX_GAMBIA,
    eCSR_COUNTRY_INDEX_GUINEA,
    eCSR_COUNTRY_INDEX_GUADELOUPE,
    eCSR_COUNTRY_INDEX_EQUATORIAL_GUINEA,
    eCSR_COUNTRY_INDEX_GREECE,
    eCSR_COUNTRY_INDEX_SOUTH_GEORGIA,
    eCSR_COUNTRY_INDEX_GUATEMALA,
    eCSR_COUNTRY_INDEX_GUAM,
    eCSR_COUNTRY_INDEX_GUINEA_BISSAU,
    eCSR_COUNTRY_INDEX_GUYANA,
    eCSR_COUNTRY_INDEX_HONGKONG,
    eCSR_COUNTRY_INDEX_HEARD_ISLAND,
    eCSR_COUNTRY_INDEX_HONDURAS,
    eCSR_COUNTRY_INDEX_CROATIA,
    eCSR_COUNTRY_INDEX_HAITI,
    eCSR_COUNTRY_INDEX_HUNGARY,
    eCSR_COUNTRY_INDEX_INDONESIA,
    eCSR_COUNTRY_INDEX_IRELAND,
    eCSR_COUNTRY_INDEX_ISRAEL,
    eCSR_COUNTRY_INDEX_ISLE_OF_MAN,
    eCSR_COUNTRY_INDEX_INDIA,
    eCSR_COUNTRY_INDEX_BRITISH_INDIAN,
    eCSR_COUNTRY_INDEX_IRAQ,
    eCSR_COUNTRY_INDEX_IRAN,
    eCSR_COUNTRY_INDEX_ICELAND,
    eCSR_COUNTRY_INDEX_ITALY,
    eCSR_COUNTRY_INDEX_JERSEY,
    eCSR_COUNTRY_INDEX_JAMAICA,
    eCSR_COUNTRY_INDEX_JORDAN,
    eCSR_COUNTRY_INDEX_JAPAN,
    eCSR_COUNTRY_INDEX_KENYA,
    eCSR_COUNTRY_INDEX_KYRGYZSTAN,
    eCSR_COUNTRY_INDEX_CAMBODIA,
    eCSR_COUNTRY_INDEX_KIRIBATI,
    eCSR_COUNTRY_INDEX_COMOROS,
    eCSR_COUNTRY_INDEX_SAINT_KITTS_AND_NEVIS,
    eCSR_COUNTRY_INDEX_KOREA_NORTH,
    eCSR_COUNTRY_INDEX_KOREA_SOUTH,
    eCSR_COUNTRY_INDEX_KUWAIT,
    eCSR_COUNTRY_INDEX_CAYMAN_ISLANDS,
    eCSR_COUNTRY_INDEX_KAZAKHSTAN,
    eCSR_COUNTRY_INDEX_LAO,
    eCSR_COUNTRY_INDEX_LEBANON,
    eCSR_COUNTRY_INDEX_SAINT_LUCIA,
    eCSR_COUNTRY_INDEX_LIECHTENSTEIN,
    eCSR_COUNTRY_INDEX_SRI_LANKA,
    eCSR_COUNTRY_INDEX_LIBERIA,
    eCSR_COUNTRY_INDEX_LESOTHO,
    eCSR_COUNTRY_INDEX_LITHUANIA,
    eCSR_COUNTRY_INDEX_LUXEMBOURG,
    eCSR_COUNTRY_INDEX_LATVIA,
    eCSR_COUNTRY_INDEX_LIBYAN_ARAB_JAMAHIRIYA,
    eCSR_COUNTRY_INDEX_MOROCCO,
    eCSR_COUNTRY_INDEX_MONACO,
    eCSR_COUNTRY_INDEX_MOLDOVA,
    eCSR_COUNTRY_INDEX_MONTENEGRO,
    eCSR_COUNTRY_INDEX_MADAGASCAR,
    eCSR_COUNTRY_INDEX_MARSHALL_ISLANDS,
    eCSR_COUNTRY_INDEX_MACEDONIA,
    eCSR_COUNTRY_INDEX_MALI,
    eCSR_COUNTRY_INDEX_MYANMAR,
    eCSR_COUNTRY_INDEX_MONGOLIA,
    eCSR_COUNTRY_INDEX_MACAO,
    eCSR_COUNTRY_INDEX_NORTHERN_MARIANA_ISLANDS,
    eCSR_COUNTRY_INDEX_MARTINIQUE,
    eCSR_COUNTRY_INDEX_MAURITANIA,
    eCSR_COUNTRY_INDEX_MONTSERRAT,
    eCSR_COUNTRY_INDEX_MALTA,
    eCSR_COUNTRY_INDEX_MAURITIUS,
    eCSR_COUNTRY_INDEX_MALDIVES,
    eCSR_COUNTRY_INDEX_MALAWI,
    eCSR_COUNTRY_INDEX_MEXICO,
    eCSR_COUNTRY_INDEX_MALAYSIA,
    eCSR_COUNTRY_INDEX_MOZAMBIQUE,
    eCSR_COUNTRY_INDEX_NAMIBIA,
    eCSR_COUNTRY_INDEX_NEW_CALENDONIA,
    eCSR_COUNTRY_INDEX_NIGER,
    eCSR_COUNTRY_INDEX_NORFOLK_ISLAND,
    eCSR_COUNTRY_INDEX_NIGERIA,
    eCSR_COUNTRY_INDEX_NICARAGUA,
    eCSR_COUNTRY_INDEX_NETHERLANDS,
    eCSR_COUNTRY_INDEX_NORWAY,
    eCSR_COUNTRY_INDEX_NEPAL,
    eCSR_COUNTRY_INDEX_NAURU,
    eCSR_COUNTRY_INDEX_NIUE,
    eCSR_COUNTRY_INDEX_NEW_ZEALAND,
    eCSR_COUNTRY_INDEX_OMAN,
    eCSR_COUNTRY_INDEX_PANAMA,
    eCSR_COUNTRY_INDEX_PERU,
    eCSR_COUNTRY_INDEX_FRENCH_POLYNESIA,
    eCSR_COUNTRY_INDEX_PAPUA_NEW_HUINEA,
    eCSR_COUNTRY_INDEX_PHILIPPINES,
    eCSR_COUNTRY_INDEX_PAKISTAN,
    eCSR_COUNTRY_INDEX_POLAND,
    eCSR_COUNTRY_INDEX_SAINT_PIERRE_AND_MIQUELON,
    eCSR_COUNTRY_INDEX_PITCAIRN,
    eCSR_COUNTRY_INDEX_PUERTO_RICO,
    eCSR_COUNTRY_INDEX_PALESTINIAN_TERRITOTY_OCCUPIED,
    eCSR_COUNTRY_INDEX_PORTUGAL,
    eCSR_COUNTRY_INDEX_PALAU,
    eCSR_COUNTRY_INDEX_PARAGUAY,
    eCSR_COUNTRY_INDEX_QATAR,
    eCSR_COUNTRY_INDEX_REUNION,
    eCSR_COUNTRY_INDEX_ROMANIA,
    eCSR_COUNTRY_INDEX_SERBIA,
    eCSR_COUNTRY_INDEX_RUSSIAN,
    eCSR_COUNTRY_INDEX_RWANDA,
    eCSR_COUNTRY_INDEX_SAUDI_ARABIA,
    eCSR_COUNTRY_INDEX_SOLOMON_ISLANDS,
    eCSR_COUNTRY_INDEX_SEYCHELLES,
    eCSR_COUNTRY_INDEX_SUDAN,
    eCSR_COUNTRY_INDEX_SWEDEN,
    eCSR_COUNTRY_INDEX_SINGAPORE,
    eCSR_COUNTRY_INDEX_SAINT_HELENA,
    eCSR_COUNTRY_INDEX_SLOVENIA,
    eCSR_COUNTRY_INDEX_SVALBARD_AND_JAN_MAYEN,
    eCSR_COUNTRY_INDEX_SLOVAKIA,
    eCSR_COUNTRY_INDEX_SIERRA_LEONE,
    eCSR_COUNTRY_INDEX_SAN_MARINO,
    eCSR_COUNTRY_INDEX_SENEGAL,
    eCSR_COUNTRY_INDEX_SOMOLIA,
    eCSR_COUNTRY_INDEX_SURINAME,
    eCSR_COUNTRY_INDEX_SAO_TOME_AND_PRINCIPE,
    eCSR_COUNTRY_INDEX_EL_SALVADOR,
    eCSR_COUNTRY_INDEX_SYRIAN_REP,
    eCSR_COUNTRY_INDEX_SWAZILAND,
    eCSR_COUNTRY_INDEX_TURKS_AND_CAICOS_ISLANDS,
    eCSR_COUNTRY_INDEX_CHAD,
    eCSR_COUNTRY_INDEX_FRENCH_SOUTHERN_TERRRTORY,
    eCSR_COUNTRY_INDEX_TOGO,
    eCSR_COUNTRY_INDEX_THAILAND,
    eCSR_COUNTRY_INDEX_TAJIKSTAN,
    eCSR_COUNTRY_INDEX_TOKELAU,
    eCSR_COUNTRY_INDEX_TIMOR_LESTE,
    eCSR_COUNTRY_INDEX_TURKMENISTAN,
    eCSR_COUNTRY_INDEX_TUNISIA,
    eCSR_COUNTRY_INDEX_TONGA,
    eCSR_COUNTRY_INDEX_TURKEY,
    eCSR_COUNTRY_INDEX_TRINIDAD_AND_TOBAGO,
    eCSR_COUNTRY_INDEX_TUVALU,
    eCSR_COUNTRY_INDEX_TAIWAN,
    eCSR_COUNTRY_INDEX_TANZANIA,
    eCSR_COUNTRY_INDEX_UKRAINE,
    eCSR_COUNTRY_INDEX_UGANDA,
    eCSR_COUNTRY_INDEX_US_MINOR_OUTLYING_ISLANDS,
    eCSR_COUNTRY_INDEX_URUGUAY,
    eCSR_COUNTRY_INDEX_UZBEKISTAN,
    eCSR_COUNTRY_INDEX_HOLY_SEE,
    eCSR_COUNTRY_INDEX_SAINT_VINCENT_AND_THE_GRENADINES,
    eCSR_COUNTRY_INDEX_VENESUELA,
    eCSR_COUNTRY_INDEX_VIRGIN_ISLANDS_BRITISH,
    eCSR_COUNTRY_INDEX_VIRGIN_ISLANDS_US,
    eCSR_COUNTRY_INDEX_VIET_NAM,
    eCSR_COUNTRY_INDEX_VANUATU,
    eCSR_COUNTRY_INDEX_WALLIS_AND_FUTUNA,
    eCSR_COUNTRY_INDEX_SAMOA,
    eCSR_COUNTRY_INDEX_YEMEN,
    eCSR_COUNTRY_INDEX_MAYOTTE,
    eCSR_COUNTRY_INDEX_SOTHER_AFRICA,
    eCSR_COUNTRY_INDEX_ZAMBIA,
    eCSR_COUNTRY_INDEX_ZIMBABWE,

    eCSR_COUNTRY_INDEX_KOREA_1,
    eCSR_COUNTRY_INDEX_KOREA_2,
    eCSR_COUNTRY_INDEX_KOREA_3,
    eCSR_COUNTRY_INDEX_KOREA_4,

    eCSR_NUM_COUNTRY_INDEX,     
}eCsrCountryIndex;
//Please donot insert in the middle of the enum above because they tie to the indiex


typedef struct tagCsrSirMBMsgHdr 
{
    tANI_U16 type;
    tANI_U16 msgLen;

}tCsrSirMBMsgHdr;

typedef struct tagCsrCfgMsgTlvHdr 
{
    tANI_U32 type;
    tANI_U32 length;    

}tCsrCfgMsgTlvHdr;



typedef struct tagCsrCfgMsgTlv
{
    tCsrCfgMsgTlvHdr Hdr;
    tANI_U32 variable[ 1 ];    // placeholder for the data
    
}tCsrCfgMsgTlv;

typedef struct tagCsrCfgGetRsp 
{
  tCsrSirMBMsgHdr hdr;
  tANI_U32    respStatus;
  tANI_U32    paramId;
  tANI_U32    attribLen;
  tANI_U32    attribVal[1];
}tCsrCfgGetRsp;

typedef struct tagCsrCfgSetRsp 
{
  
  tCsrSirMBMsgHdr hdr;
  tANI_U32    respStatus;
  tANI_U32    paramId;
}tCsrCfgSetRsp;


typedef struct tagCsrDomainChnScanInfo
{
    tANI_U8 chnId;
    tSirScanType scanType;  //whether this channel must be scan passively
}tCsrDomainChnScanInfo;


#if defined(__ANI_COMPILER_PRAGMA_PACK_STACK)
#pragma pack( push )
#pragma pack( 1 )
#elif defined(__ANI_COMPILER_PRAGMA_PACK)
#pragma pack( 1 )
#endif

// Generic Information Element Structure
typedef __ani_attr_pre_packed struct sDot11IEHeader 
{
    tANI_U8 ElementID;
    tANI_U8 Length;
}__ani_attr_packed tDot11IEHeader;

typedef __ani_attr_pre_packed struct tagCsrWmeInfoIe
{
    tDot11IEHeader IeHeader;
    tANI_U8    Oui[ CSR_WME_OUI_SIZE ];  // includes the 3 byte OUI + 1 byte Type
    tANI_U8    Subtype;
    tANI_U8    Version;
    tANI_U8    QoSInfo;
    
} __ani_attr_packed tCsrWmeInfoIe;

typedef __ani_attr_pre_packed struct tagCsrWmeAcParms
{
    tANI_U8  AciAifsn;
    tANI_U8  EcwMinEcwMax;
    tANI_U16 TxOpLimit;
    
} __ani_attr_packed tCsrWmeAcParms;

typedef __ani_attr_pre_packed struct tagCsrWmeParmIe
{
    tDot11IEHeader IeHeader;
    tANI_U8    Oui[ CSR_WME_OUI_SIZE ];  // includes the 3 byte OUI + 1 byte Type
    tANI_U8    Subtype;
    tANI_U8    Version;
    tANI_U8    QoSInfo;
    tANI_U8    Reserved;
    tCsrWmeAcParms   BestEffort;
    tCsrWmeAcParms   Background;
    tCsrWmeAcParms   Video;
    tCsrWmeAcParms   Voice;
        
} __ani_attr_packed tCsrWmeParmIe;

typedef __ani_attr_pre_packed struct tagCsrWpaIe 
{
    tDot11IEHeader IeHeader;
    tANI_U8    Oui[ CSR_WPA_OUI_SIZE ];
    tANI_U16   Version;
    tANI_U8    MulticastOui[ CSR_WPA_OUI_SIZE ];
    tANI_U16   cUnicastCyphers;
    
    __ani_attr_pre_packed struct {
    
        tANI_U8 Oui[ CSR_WPA_OUI_SIZE ];
        
    } __ani_attr_packed UnicastOui[ 1 ];
    
} __ani_attr_packed tCsrWpaIe;

typedef __ani_attr_pre_packed struct tagCsrWpaAuthIe 
{

    tANI_U16 cAuthenticationSuites;
    
    __ani_attr_pre_packed struct {
    
        tANI_U8 Oui[ CSR_WPA_OUI_SIZE ];
    
    } __ani_attr_packed AuthOui[ 1 ];

} __ani_attr_packed tCsrWpaAuthIe;


typedef __ani_attr_pre_packed struct tagCsrRSNIe 
{
    tDot11IEHeader IeHeader;
    tANI_U16   Version;
    tANI_U8    MulticastOui[ CSR_RSN_OUI_SIZE ];
    tANI_U16   cUnicastCyphers;
    
    __ani_attr_pre_packed struct {
    
        tANI_U8 Oui[ CSR_RSN_OUI_SIZE ];
        
    } __ani_attr_packed UnicastOui[ 1 ];
    
} __ani_attr_packed tCsrRSNIe;

typedef __ani_attr_pre_packed struct tagCsrRSNAuthIe 
{
    tANI_U16 cAuthenticationSuites;
    __ani_attr_pre_packed struct {
    
        tANI_U8 Oui[ CSR_RSN_OUI_SIZE ];
    
    } __ani_attr_packed AuthOui[ 1 ];

} __ani_attr_packed tCsrRSNAuthIe;

typedef __ani_attr_pre_packed struct tagCsrRSNCapabilities 
{
    tANI_U16 PreAuthSupported:1;
    tANI_U16 NoPairwise:1;
    tANI_U16 PTKSAReplayCounter:2;
    tANI_U16 GTKSAReplayCounter:2;
    tANI_U16 MFPRequired:1;
    tANI_U16 MFPCapable:1;
    tANI_U16 Reserved:8;
} __ani_attr_packed tCsrRSNCapabilities;

typedef __ani_attr_pre_packed struct tagCsrRSNPMKIe 
{
    tANI_U16 cPMKIDs;
    
    __ani_attr_pre_packed struct {
    
        tANI_U8 PMKID[ CSR_RSN_PMKID_SIZE ];
    
    } __ani_attr_packed PMKIDList[ 1 ];


} __ani_attr_packed tCsrRSNPMKIe;

typedef __ani_attr_pre_packed struct tCsrIELenInfo
{
    tANI_U8 min;
    tANI_U8 max;
} __ani_attr_packed tCsrIELenInfo;

#ifdef FEATURE_WLAN_WAPI
typedef __ani_attr_pre_packed struct tagCsrWapiIe 
{
    tDot11IEHeader IeHeader;
    tANI_U16   Version;

    tANI_U16 cAuthenticationSuites;
    __ani_attr_pre_packed struct {

        tANI_U8 Oui[ CSR_WAPI_OUI_SIZE ];

    } __ani_attr_packed AuthOui[ 1 ];

    tANI_U16   cUnicastCyphers;
    __ani_attr_pre_packed struct {

        tANI_U8 Oui[ CSR_WAPI_OUI_SIZE ];

    } __ani_attr_packed UnicastOui[ 1 ];

    tANI_U8    MulticastOui[ CSR_WAPI_OUI_SIZE ];    

    __ani_attr_pre_packed struct {
       tANI_U16 PreAuthSupported:1;
       tANI_U16 Reserved:15;
    } __ani_attr_packed tCsrWapiCapabilities;


} __ani_attr_packed tCsrWapiIe;

typedef __ani_attr_pre_packed struct tagCsrWAPIBKIe 
{
    tANI_U16 cBKIDs;
    __ani_attr_pre_packed struct {

        tANI_U8 BKID[ CSR_WAPI_BKID_SIZE ];

    } __ani_attr_packed BKIDList[ 1 ];


} __ani_attr_packed tCsrWAPIBKIe;
#endif /* FEATURE_WLAN_WAPI */

#if defined(__ANI_COMPILER_PRAGMA_PACK_STACK)
#pragma pack( pop )
#endif

// Structure used to describe a group of continuous channels and hook it into the 
// corresponding channel list
typedef struct tagCsrChannelSet 
{
  tListElem      channelListLink;
  tANI_U8   firstChannel;
  tANI_U8   interChannelOffset;
  tANI_U8   numChannels;
  tANI_U8   txPower;
}tCsrChannelSet;


typedef struct sDot11InfoIBSSParmSet 
{
    tDot11IEHeader dot11IEHeader;
    tANI_U8 ATIMWindow;
}tDot11InfoIBSSParmSet;


typedef struct sDot11IECountry 
{
    tDot11IEHeader dot11IEHeader;
    tANI_U8 countryString[3];
    tSirMacChanInfo chanInfo[1];
}tDot11IECountry;


typedef struct sDot11IEExtenedSupportedRates 
{
    tDot11IEHeader dot11IEHeader;
    tANI_U8 ExtendedSupportedRates[ CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX ];
}tDot11IEExtenedSupportedRates;

#define CSR_DOT11_AP_NAME_MAX_LENGTH ( 32 )

typedef struct tagDot11IEAPName
{
    tDot11IEHeader dot11IEHeader;
    tANI_U8 ApName[ CSR_DOT11_AP_NAME_MAX_LENGTH ];
}tDot11IEAPName;

typedef struct tagDot11IE11HLocalPowerConstraint
{
    tDot11IEHeader dot11IEHeader;
    tANI_U8 localPowerConstraint;

}tDot11IE11HLocalPowerConstraint;

typedef struct tagRoamingTimerInfo
{
    tpAniSirGlobal pMac;
    tANI_U8 sessionId;
} tCsrTimerInfo;


#define CSR_IS_11A_BSS(pBssDesc)    ( eSIR_11A_NW_TYPE == (pBssDesc)->nwType )
#define CSR_IS_BASIC_RATE(rate)     ((rate) & CSR_DOT11_BASIC_RATE_MASK)
#define CSR_IS_QOS_BSS(pIes)  ( (pIes)->WMMParams.present || (pIes)->WMMInfoAp.present )

#define CSR_IS_UAPSD_BSS(pIes) \
    ( ((pIes)->WMMParams.present && ((pIes)->WMMParams.qosInfo & SME_QOS_AP_SUPPORTS_APSD)) || \
               ((pIes)->WMMInfoAp.present && (pIes)->WMMInfoAp.uapsd) )

//This macro returns the total length needed of Tlv with with len bytes of data
#define GET_TLV_MSG_LEN(len)    GET_ROUND_UP((sizeof(tCsrCfgMsgTlvHdr) + (len)), sizeof(tANI_U32))

tANI_BOOLEAN csrGetBssIdBssDesc( tHalHandle hHal, tSirBssDescription *pSirBssDesc, tCsrBssid *pBssId ); 
tANI_BOOLEAN csrIsBssIdEqual( tHalHandle hHal, tSirBssDescription *pSirBssDesc1, tSirBssDescription *pSirBssDesc2 );

eCsrMediaAccessType csrGetQoSFromBssDesc( tHalHandle hHal, tSirBssDescription *pSirBssDesc,
                                          tDot11fBeaconIEs *pIes);
tANI_BOOLEAN csrIsNULLSSID( tANI_U8 *pBssSsid, tANI_U8 len );
tANI_BOOLEAN csrIsInfraBssDesc( tSirBssDescription *pSirBssDesc ); 
tANI_BOOLEAN csrIsIbssBssDesc( tSirBssDescription *pSirBssDesc ); 
tANI_BOOLEAN csrIsPrivacy( tSirBssDescription *pSirBssDesc );
tSirResultCodes csrGetDisassocRspStatusCode( tSirSmeDisassocRsp *pSmeDisassocRsp );
tSirResultCodes csrGetDeAuthRspStatusCode( tSirSmeDeauthRsp *pSmeRsp );
tANI_U32 csrGetFragThresh( tHalHandle hHal );
tANI_U32 csrGetRTSThresh( tHalHandle hHal );
eCsrPhyMode csrGetPhyModeFromBssDesc( tSirBssDescription *pSirBssDesc );
tANI_U32 csrGet11hPowerConstraint( tHalHandle hHal, tDot11fIEPowerConstraints *pPowerConstraint );
tANI_U8 csrConstructRSNIe( tHalHandle hHal, tANI_U32 sessionId, tCsrRoamProfile *pProfile, 
                            tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes, tCsrRSNIe *pRSNIe );
tANI_U8 csrConstructWpaIe( tHalHandle hHal, tCsrRoamProfile *pProfile, tSirBssDescription *pSirBssDesc, 
                           tDot11fBeaconIEs *pIes, tCsrWpaIe *pWpaIe );
#ifdef FEATURE_WLAN_WAPI

tANI_BOOLEAN csrIsProfileWapi( tCsrRoamProfile *pProfile );
#endif /* FEATURE_WLAN_WAPI */
//If a WPAIE exists in the profile, just use it. Or else construct one from the BSS
//Caller allocated memory for pWpaIe and guarrantee it can contain a max length WPA IE
tANI_U8 csrRetrieveWpaIe( tHalHandle hHal, tCsrRoamProfile *pProfile, tSirBssDescription *pSirBssDesc, 
                          tDot11fBeaconIEs *pIes, tCsrWpaIe *pWpaIe );
tANI_BOOLEAN csrIsSsidEqual( tHalHandle hHal, tSirBssDescription *pSirBssDesc1, 
                             tSirBssDescription *pSirBssDesc2, tDot11fBeaconIEs *pIes2 );
//Null ssid means match
tANI_BOOLEAN csrIsSsidInList( tHalHandle hHal, tSirMacSSid *pSsid, tCsrSSIDs *pSsidList );
tANI_BOOLEAN csrIsProfileWpa( tCsrRoamProfile *pProfile );
tANI_BOOLEAN csrIsProfileRSN( tCsrRoamProfile *pProfile );
//This function returns the raw byte array of WPA and/or RSN IE
tANI_BOOLEAN csrGetWpaRsnIe( tHalHandle hHal, tANI_U8 *pIes, tANI_U32 len,
                             tANI_U8 *pWpaIe, tANI_U8 *pcbWpaIe, tANI_U8 *pRSNIe, tANI_U8 *pcbRSNIe);
//If a RSNIE exists in the profile, just use it. Or else construct one from the BSS
//Caller allocated memory for pWpaIe and guarrantee it can contain a max length WPA IE
tANI_U8 csrRetrieveRsnIe( tHalHandle hHal, tANI_U32 sessionId, tCsrRoamProfile *pProfile, tSirBssDescription *pSirBssDesc, 
                          tDot11fBeaconIEs *pIes, tCsrRSNIe *pRsnIe );
#ifdef FEATURE_WLAN_WAPI
//If a WAPI IE exists in the profile, just use it. Or else construct one from the BSS
//Caller allocated memory for pWapiIe and guarrantee it can contain a max length WAPI IE
tANI_U8 csrRetrieveWapiIe( tHalHandle hHal, tANI_U32 sessionId, tCsrRoamProfile *pProfile, tSirBssDescription *pSirBssDesc, 
                          tDot11fBeaconIEs *pIes, tCsrWapiIe *pWapiIe );
#endif /* FEATURE_WLAN_WAPI */
tANI_BOOLEAN csrSearchChannelListForTxPower(tHalHandle hHal, tSirBssDescription *pBssDescription, tCsrChannelSet *returnChannelGroup);
tANI_BOOLEAN csrRatesIsDot11Rate11bSupportedRate( tANI_U8 dot11Rate );
tANI_BOOLEAN csrRatesIsDot11Rate11aSupportedRate( tANI_U8 dot11Rate );
tAniEdType csrTranslateEncryptTypeToEdType( eCsrEncryptionType EncryptType ); 
//pIes shall contain IEs from pSirBssDesc. It shall be returned from function csrGetParsedBssDescriptionIEs 
tANI_BOOLEAN csrIsSecurityMatch( tHalHandle hHal, tCsrAuthList *authType,
                                 tCsrEncryptionList *pUCEncryptionType,
                                 tCsrEncryptionList *pMCEncryptionType,
                                 tANI_BOOLEAN *pMFPEnabled,
                                 tANI_U8 *pMFPRequired,
                                 tANI_U8 *pMFPCapable,
                                 tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes, 
                                 eCsrAuthType *negotiatedAuthtype, eCsrEncryptionType *negotiatedUCCipher, eCsrEncryptionType *negotiatedMCCipher );
tANI_BOOLEAN csrIsBSSTypeMatch(eCsrRoamBssType bssType1, eCsrRoamBssType bssType2);
tANI_BOOLEAN csrIsBssTypeIBSS(eCsrRoamBssType bssType);
tANI_BOOLEAN csrIsBssTypeWDS(eCsrRoamBssType bssType);
//ppIes can be NULL. If caller want to get the *ppIes allocated by this function, pass in *ppIes = NULL
//Caller needs to free the memory in this case
tANI_BOOLEAN csrMatchBSS( tHalHandle hHal, tSirBssDescription *pBssDesc, tCsrScanResultFilter *pFilter, 
                          eCsrAuthType *pNegAuth, eCsrEncryptionType *pNegUc, eCsrEncryptionType *pNegMc,
                          tDot11fBeaconIEs **ppIes);

tANI_BOOLEAN csrIsBssidMatch( tHalHandle hHal, tCsrBssid *pProfBssid, tCsrBssid *BssBssid );
tANI_BOOLEAN csrMatchBSSToConnectProfile( tHalHandle hHal, tCsrRoamConnectedProfile *pProfile,
                                          tSirBssDescription *pBssDesc, tDot11fBeaconIEs *pIes );
tANI_BOOLEAN csrRatesIsDot11RateSupported( tHalHandle hHal, tANI_U8 rate );
tANI_U16 csrRatesFindBestRate( tSirMacRateSet *pSuppRates, tSirMacRateSet *pExtRates, tSirMacPropRateSet *pPropRates );
tSirBssType csrTranslateBsstypeToMacType(eCsrRoamBssType csrtype);
                            
//Caller allocates memory for pIEStruct
eHalStatus csrParseBssDescriptionIEs(tHalHandle hHal, tSirBssDescription *pBssDesc, tDot11fBeaconIEs *pIEStruct);
//This function will allocate memory for the parsed IEs to the caller. Caller must free the memory
//after it is done with the data only if this function succeeds
eHalStatus csrGetParsedBssDescriptionIEs(tHalHandle hHal, tSirBssDescription *pBssDesc, tDot11fBeaconIEs **ppIEStruct);

tANI_BOOLEAN csrValidateCountryString( tHalHandle hHal, tANI_U8 *pCountryString );
tSirScanType csrGetScanType(tpAniSirGlobal pMac, tANI_U8 chnId);

tANI_U8 csrToUpper( tANI_U8 ch );
eHalStatus csrGetPhyModeFromBss(tpAniSirGlobal pMac, tSirBssDescription *pBSSDescription, 
                                eCsrPhyMode *pPhyMode, tDot11fBeaconIEs *pIes);

//fForce -- force reassoc regardless of whether there is any change
//The reason is that for UAPSD-bypass, the code underneath this call determine whether
//to allow UAPSD. The information in pModProfileFields reflects what the user wants.
//There may be discrepency in it. UAPSD-bypass logic should decide if it needs to reassoc
eHalStatus csrReassoc(tpAniSirGlobal pMac, tANI_U32 sessionId,
                      tCsrRoamModifyProfileFields *pModProfileFields,
                      tANI_U32 *pRoamId, v_BOOL_t fForce);

eHalStatus
csrIsconcurrentsessionValid(tpAniSirGlobal pMac,tANI_U32 cursessionId,
                                 tVOS_CON_MODE currBssPersona);

//Update beaconInterval for P2P-GO case if it is different 
eHalStatus csrUpdatep2pBeaconInterval(tpAniSirGlobal pMac);

//BeaconInterval validation for MCC support
eHalStatus csrValidateMCCBeaconInterval(tpAniSirGlobal pMac, tANI_U8 channelId,
                                     tANI_U16 *beaconInterval, tANI_U32 cursessionId,
                                     tVOS_CON_MODE currBssPersona);

#ifdef WLAN_FEATURE_VOWIFI_11R
tANI_BOOLEAN csrIsProfile11r( tCsrRoamProfile *pProfile );
tANI_BOOLEAN csrIsAuthType11r( eCsrAuthType AuthType, tANI_U8 mdiePresent);
#endif

#ifdef FEATURE_WLAN_ESE
tANI_BOOLEAN csrIsAuthTypeESE( eCsrAuthType AuthType );
tANI_BOOLEAN csrIsProfileESE( tCsrRoamProfile *pProfile );
#endif

#endif

