/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _P2P_H
#define _P2P_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/* refer to 'Config Methods' in WPS */
#define WPS_CONFIG_USBA                 0x0001
#define WPS_CONFIG_ETHERNET             0x0002
#define WPS_CONFIG_LABEL                0x0004
#define WPS_CONFIG_DISPLAY              0x0008
#define WPS_CONFIG_EXT_NFC              0x0010
#define WPS_CONFIG_INT_NFC              0x0020
#define WPS_CONFIG_NFC                  0x0040
#define WPS_CONFIG_PBC                  0x0080
#define WPS_CONFIG_KEYPAD               0x0100

/* refer to 'Device Password ID' in WPS */
#define WPS_DEV_PASSWORD_ID_PIN         0x0000
#define WPS_DEV_PASSWORD_ID_USER        0x0001
#define WPS_DEV_PASSWORD_ID_MACHINE     0x0002
#define WPS_DEV_PASSWORD_ID_REKEY       0x0003
#define WPS_DEV_PASSWORD_ID_PUSHBUTTON  0x0004
#define WPS_DEV_PASSWORD_ID_REGISTRAR   0x0005

#define P2P_DEVICE_TYPE_NUM         2
#define P2P_DEVICE_NAME_LENGTH      32
#define P2P_NETWORK_NUM             8
#define P2P_MEMBER_NUM              8

#define P2P_WILDCARD_SSID           "DIRECT-"

#define MAX_GC_DEAUTH_RETRY_COUNT   1

#define P2P_DEAUTH_TIMEOUT_TIME_MS 1000

#define P2P_AP_CHNL_HOLD_TIME_MS 5000

#define P2P_CHNL_EXTEND_CHAN_TIME 500

#define AP_DEFAULT_CHANNEL_2G     6
#define AP_DEFAULT_CHANNEL_5G     36

#define P2P_MAX_AKM_SUITES 2

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

struct _P2P_INFO_T {
	UINT_32 u4DeviceNum;
	EVENT_P2P_DEV_DISCOVER_RESULT_T arP2pDiscoverResult[CFG_MAX_NUM_BSS_LIST];
	PUINT_8 pucCurrIePtr;
	UINT_8 aucCommIePool[CFG_MAX_COMMON_IE_BUF_LEN];	/* A common pool for IE of all scan results. */
};

typedef enum {
	ENUM_P2P_PEER_GROUP,
	ENUM_P2P_PEER_DEVICE,
	ENUM_P2P_PEER_NUM
} ENUM_P2P_PEER_TYPE, *P_ENUM_P2P_PEER_TYPE;

typedef struct _P2P_DEVICE_INFO {
	UINT_8 aucDevAddr[PARAM_MAC_ADDR_LEN];
	UINT_8 aucIfAddr[PARAM_MAC_ADDR_LEN];
	UINT_8 ucDevCapabilityBitmap;
	INT_32 i4ConfigMethod;
	UINT_8 aucPrimaryDeviceType[8];
	UINT_8 aucSecondaryDeviceType[8];
	UINT_8 aucDeviceName[P2P_DEVICE_NAME_LENGTH];
} P2P_DEVICE_INFO, *P_P2P_DEVICE_INFO;

typedef struct _P2P_GROUP_INFO {
	PARAM_SSID_T rGroupID;
	P2P_DEVICE_INFO rGroupOwnerInfo;
	UINT_8 ucMemberNum;
	P2P_DEVICE_INFO arMemberInfo[P2P_MEMBER_NUM];
} P2P_GROUP_INFO, *P_P2P_GROUP_INFO;

typedef struct _P2P_NETWORK_INFO {
	ENUM_P2P_PEER_TYPE eNodeType;

	union {
		P2P_GROUP_INFO rGroupInfo;
		P2P_DEVICE_INFO rDeviceInfo;
	} node;

} P2P_NETWORK_INFO, *P_P2P_NETWORK_INFO;

typedef struct _P2P_NETWORK_LIST {
	UINT_8 ucNetworkNum;
	P2P_NETWORK_INFO rP2PNetworkInfo[P2P_NETWORK_NUM];
} P2P_NETWORK_LIST, *P_P2P_NETWORK_LIST;

typedef struct _P2P_DISCONNECT_INFO {
	UINT_8 ucRole;
	UINT_8 ucRsv[3];
} P2P_DISCONNECT_INFO, *P_P2P_DISCONNECT_INFO;

/* P2P public action frames */
enum P2P_ACTION_FRAME_TYPE {
	P2P_GO_NEG_REQ = 0,
	P2P_GO_NEG_RESP = 1,
	P2P_GO_NEG_CONF = 2,
	P2P_INVITATION_REQ = 3,
	P2P_INVITATION_RESP = 4,
	P2P_DEV_DISC_REQ = 5,
	P2P_DEV_DISC_RESP = 6,
	P2P_PROV_DISC_REQ = 7,
	P2P_PROV_DISC_RESP = 8
};

struct P2P_QUEUED_ACTION_FRAME {
	int32_t u4Freq;
	uint8_t *prHeader;
	uint16_t u2Length;
};

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

#endif /*_P2P_H */
