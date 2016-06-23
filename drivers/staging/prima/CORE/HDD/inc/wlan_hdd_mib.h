/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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

#if !defined( WLAN_HDD_MIB_h__ )
#define WLAN_HDD_MIB_h__ 


#include <vos_types.h>

typedef enum
{
    eMib_dot11DesiredBssType_infrastructure = 1,
    eMib_dot11DesiredBssType_independent = 2,
    eMib_dot11DesiredBssType_infra_ap =3,
    eMib_dot11DesiredBssType_any = 4
    
} eMib_dot11DesiredBssType;


/** This is the maximum number of BSSIDs supported in the 
      dot11DesiredBssidList.  All the code operates off of 
      this maximum BSSID list count.  */
#define MIB_DOT11_DESIRED_BSSID_LIST_MAX_COUNT ( 1 )

typedef struct
{
    v_U32_t cEntries;
    
    v_MACADDR_t BSSIDs[ MIB_DOT11_DESIRED_BSSID_LIST_MAX_COUNT ];     

}  sMib_dot11DesiredBssidList;



/** This is the maximum number of SSIDs supported in the 
     dot11DesiredSsidList.  All the code operates off of 
     this maximum SSID list count.  */
     
#define MIB_DOT11_DESIRED_SSID_LIST_MAX_COUNT ( 1 )

#define MIB_DOT11_SSID_MAX_LENGTH ( 32 )

typedef struct
{
    v_U32_t ssidLength;
    v_U8_t  ssid[ MIB_DOT11_SSID_MAX_LENGTH ];
    
} sDot11Ssid;

typedef struct
{
    v_U32_t cEntries;
    
    sDot11Ssid SSIDs[ MIB_DOT11_DESIRED_SSID_LIST_MAX_COUNT ];     

}  sMib_dot11DesiredSsidList;



typedef enum
{
    // these are bitmasks....
    eMib_dot11AutoConfigEnabled_None = 0U,
    eMib_dot11AutoConfigEnabled_Phy  = 0x00000001U,
    eMib_dot11AutoConfigEnabled_Mac  = 0x00000002U
    
} eMib_dot11AutoConfigEnabled;



#define MIB_DOT11_SUPPORTED_PHY_TYPES_MAX_COUNT ( 3 )

typedef enum tagMib_dot11PhyType 
{
    eMib_dot11PhyType_11b,
    eMib_dot11PhyType_11a,
    eMib_dot11PhyType_11g,
    eMib_dot11PhyType_all
} eMib_dot11PhyType;

typedef struct tagMib_dot11SupportedPhyTypes
{
    v_U32_t cEntries;    
    eMib_dot11PhyType phyTypes[ MIB_DOT11_SUPPORTED_PHY_TYPES_MAX_COUNT ];     
}  sMib_dot11SupportedPhyTypes;


typedef enum
{
   eMib_DevicePowerState_D0, 
   eMib_DevicePowerState_D1, 
   eMib_DevicePowerState_D2, 
   eMib_DevicePowerState_D3
   
} eMib_DevicePowerState;    
   

typedef enum
{
    eMib_dot11NICPowerState_OFF = VOS_FALSE,
    eMib_dot11NICPowerState_ON  = VOS_TRUE
    
} eMib_dot11NICPowerState;


typedef enum
{
    eMib_dot11HardwarePHYState_OFF = VOS_FALSE,
    eMib_dot11HardwarePHYState_ON  = VOS_TRUE
    
} eMib_dot11HardwarePHYState;


typedef enum
{
    eMib_dot11PowerSavingLevel_None, 
    eMib_dot11PowerSavingLevel_MaxPS,
    eMib_dot11PowerSavingLevel_FastPS,
    eMib_dot11PowerSavingLevel_MaximumLevel
    
} eMib_dot11PowerSavingLevel;    


#define MIB_DOT11_MAC_EXCLUSION_LIST_MAX_COUNT 4
typedef struct
{
    v_U32_t cEntries;
    
    v_MACADDR_t macAddrs[ MIB_DOT11_MAC_EXCLUSION_LIST_MAX_COUNT ];     

} sMib_dot11MacExcludeList;

#define MIB_DOT11_PRIVACY_EXEMPT_LIST_MAX_COUNT 32

typedef enum
{
    eMib_dot11ExemptionAction_Always,
    eMib_dot11ExemptionAction_OnKeyMapUnavailable

}eMib_dot11ExemptAction;

typedef enum
{
    eMib_dot11ExemptPacket_Unicast,
    eMib_dot11ExemptPacket_Multicast,
    eMib_dot11ExemptPacket_Both

}eMib_dot11ExemptPacket;

typedef struct
{
    v_U16_t uEtherType;
    eMib_dot11ExemptAction exemptAction;
    eMib_dot11ExemptPacket exemptPacket;

}sMib_dot11PrivacyExemption;

typedef struct
{
    v_U32_t cEntries;

    sMib_dot11PrivacyExemption privacyExemptList[ MIB_DOT11_PRIVACY_EXEMPT_LIST_MAX_COUNT ];

} sMib_dot11PrivacyExemptionList;

typedef struct sHddMib_s
{
    eMib_dot11DesiredBssType    mibDot11DesiredBssType;

    sMib_dot11DesiredBssidList  mibDot11DesiredBssidList;

    sMib_dot11DesiredSsidList   mibDot11DesiredSsidList;

    eMib_dot11AutoConfigEnabled mibDot11AutoConfigEnabled;

    // the device power state for the device (the D-state... you know D0, D1, D2, etc.
    eMib_DevicePowerState      mibDevicePowerState;

    // dot11NICPowerState is really the on/off state of the PHY.  This can be
    // mamipulated through OIDs like a software control for radio on/off.
    eMib_dot11NICPowerState    mibDot11NICPowerState;

    // Hardware PHY state is the on/off state of the hardware PHY.
    eMib_dot11HardwarePHYState mibDot11HardwarePHYState;

    // dot11 Power Saving level is the 802.11 power saving level/state for the 802.11
    // NIC.  Typically this is mapped to 802.11 BMPS in some fashion.  We are not going
    // to disappoint; the Libra NIC maps these to different BMPS settings.
    eMib_dot11PowerSavingLevel mibDot11PowerSavingLevel;

    sMib_dot11MacExcludeList mibDot11MacExcludeList;

    sMib_dot11PrivacyExemptionList mibDot11PrivacyExemptionList;

    sMib_dot11SupportedPhyTypes  mibDot11SupportedPhyTypes;
    eMib_dot11PhyType            mibDot11CurrentPhyType;

    v_BOOL_t                      dot11IbssJoinOnly;
    v_BOOL_t                      HiddenNetworkEnabled;


}sHddMib_t;

#endif
