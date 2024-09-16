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

#ifndef _BOW_H_
#define _BOW_H_

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

#define BOWDEVNAME          "bow0"

#define MAX_BOW_NUMBER_OF_CHANNEL_2G4            14
#define MAX_BOW_NUMBER_OF_CHANNEL_5G              4
/* (MAX_BOW_NUMBER_OF_CHANNEL_2G4 + MAX_BOW_NUMBER_OF_CHANNEL_5G) */
#define MAX_BOW_NUMBER_OF_CHANNEL                    18

#define MAX_ACTIVITY_REPORT                                    2
#define MAX_ACTIVITY_REPROT_TIME                          660

#define ACTIVITY_REPORT_STATUS_SUCCESS              0
#define ACTIVITY_REPORT_STATUS_FAILURE               1
#define ACTIVITY_REPORT_STATUS_TIME_INVALID     2
#define ACTIVITY_REPORT_STATUS_OTHERS                3

#define ACTIVITY_REPORT_SCHEDULE_UNKNOWN        0	/* Does not know the schedule of the interference */
#define ACTIVITY_REPORT_SCHEDULE_KNOWN             1

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/
typedef struct _BT_OVER_WIFI_COMMAND_HEADER_T {
	UINT_8 ucCommandId;
	UINT_8 ucSeqNumber;
	UINT_16 u2PayloadLength;
} AMPC_COMMAND_HEADER_T, *P_AMPC_COMMAND_HEADER_T;

typedef struct _BT_OVER_WIFI_COMMAND {
	AMPC_COMMAND_HEADER_T rHeader;
	UINT_8 aucPayload[0];
} AMPC_COMMAND, *P_AMPC_COMMAND;

typedef struct _BT_OVER_WIFI_EVENT_HEADER_T {
	UINT_8 ucEventId;
	UINT_8 ucSeqNumber;
	UINT_16 u2PayloadLength;
} AMPC_EVENT_HEADER_T, *P_AMPC_EVENT_HEADER_T;

typedef struct _BT_OVER_WIFI_EVENT {
	AMPC_EVENT_HEADER_T rHeader;
	UINT_8 aucPayload[0];
} AMPC_EVENT, *P_AMPC_EVENT;

typedef struct _CHANNEL_DESC_T {
	UINT_8 ucChannelBand;
	UINT_8 ucChannelNum;
} CHANNEL_DESC, P_CHANNEL_DESC;

/* Command Structures */
typedef struct _BOW_SETUP_CONNECTION {
/* Fixed to 2.4G */
	UINT_8 ucChannelNum;
	UINT_8 ucReserved1;
	UINT_8 aucPeerAddress[6];
	UINT_16 u2BeaconInterval;
	UINT_8 ucTimeoutDiscovery;
	UINT_8 ucTimeoutInactivity;
	UINT_8 ucRole;
	UINT_8 ucPAL_Capabilities;
	INT_8 cMaxTxPower;
	UINT_8 ucReserved2;

/* Pending, for future BOW 5G supporting. */
/*
 * UINT_8          aucPeerAddress[6];
 * UINT_16         u2BeaconInterval;
 * UINT_8          ucTimeoutDiscovery;
 * UINT_8          ucTimeoutInactivity;
 * UINT_8          ucRole;
 * UINT_8          ucPAL_Capabilities;
 * INT_8           cMaxTxPower;
 * UINT_8          ucChannelListNum;
 * CHANNEL_DESC    arChannelList[1];
 */
} BOW_SETUP_CONNECTION, *P_BOW_SETUP_CONNECTION;

typedef struct _BOW_DESTROY_CONNECTION {
	UINT_8 aucPeerAddress[6];
	UINT_8 aucReserved[2];
} BOW_DESTROY_CONNECTION, *P_BOW_DESTROY_CONNECTION;

typedef struct _BOW_SET_PTK {
	UINT_8 aucPeerAddress[6];
	UINT_8 aucReserved[2];
	UINT_8 aucTemporalKey[16];
} BOW_SET_PTK, *P_BOW_SET_PTK;

typedef struct _BOW_READ_RSSI {
	UINT_8 aucPeerAddress[6];
	UINT_8 aucReserved[2];
} BOW_READ_RSSI, *P_BOW_READ_RSSI;

typedef struct _BOW_READ_LINK_QUALITY {
	UINT_8 aucPeerAddress[6];
	UINT_8 aucReserved[2];
} BOW_READ_LINK_QUALITY, *P_BOW_READ_LINK_QUALITY;

typedef struct _BOW_SHORT_RANGE_MODE {
	UINT_8 aucPeerAddress[6];
	INT_8 cTxPower;
	UINT_8 ucReserved;
} BOW_SHORT_RANGE_MODE, *P_BOW_SHORT_RANGE_MODE;

/* Event Structures */
typedef struct _BOW_COMMAND_STATUS {
	UINT_8 ucStatus;
	UINT_8 ucReserved[3];
} BOW_COMMAND_STATUS, *P_BOW_COMMAND_STATUS;

typedef struct _BOW_MAC_STATUS {
	UINT_8 aucMacAddr[6];
	UINT_8 ucAvailability;
	UINT_8 ucNumOfChannel;
	CHANNEL_DESC arChannelList[MAX_BOW_NUMBER_OF_CHANNEL];
} BOW_MAC_STATUS, *P_BOW_MAC_STATUS;

typedef struct _BOW_LINK_CONNECTED {
	CHANNEL_DESC rChannel;
	UINT_8 aucReserved;
	UINT_8 aucPeerAddress[6];
} BOW_LINK_CONNECTED, *P_BOW_LINK_CONNECTED;

typedef struct _BOW_LINK_DISCONNECTED {
	UINT_8 ucReason;
	UINT_8 aucReserved;
	UINT_8 aucPeerAddress[6];
} BOW_LINK_DISCONNECTED, *P_BOW_LINK_DISCONNECTED;

typedef struct _BOW_RSSI {
	INT_8 cRssi;
	UINT_8 aucReserved[3];
} BOW_RSSI, *P_BOW_RSSI;

typedef struct _BOW_LINK_QUALITY {
	UINT_8 ucLinkQuality;
	UINT_8 aucReserved[3];
} BOW_LINK_QUALITY, *P_BOW_LINK_QUALITY;

typedef enum _ENUM_BOW_CMD_ID_T {
	BOW_CMD_ID_GET_MAC_STATUS = 1,
	BOW_CMD_ID_SETUP_CONNECTION,
	BOW_CMD_ID_DESTROY_CONNECTION,
	BOW_CMD_ID_SET_PTK,
	BOW_CMD_ID_READ_RSSI,
	BOW_CMD_ID_READ_LINK_QUALITY,
	BOW_CMD_ID_SHORT_RANGE_MODE,
	BOW_CMD_ID_GET_CHANNEL_LIST,
} ENUM_BOW_CMD_ID_T, *P_ENUM_BOW_CMD_ID_T;

typedef enum _ENUM_BOW_EVENT_ID_T {
	BOW_EVENT_ID_COMMAND_STATUS = 1,
	BOW_EVENT_ID_MAC_STATUS,
	BOW_EVENT_ID_LINK_CONNECTED,
	BOW_EVENT_ID_LINK_DISCONNECTED,
	BOW_EVENT_ID_RSSI,
	BOW_EVENT_ID_LINK_QUALITY,
	BOW_EVENT_ID_CHANNEL_LIST,
	BOW_EVENT_ID_CHANNEL_SELECTED,
} ENUM_BOW_EVENT_ID_T, *P_ENUM_BOW_EVENT_ID_T;

typedef enum _ENUM_BOW_DEVICE_STATE {
	BOW_DEVICE_STATE_DISCONNECTED = 0,
	BOW_DEVICE_STATE_DISCONNECTING,
	BOW_DEVICE_STATE_ACQUIRING_CHANNEL,
	BOW_DEVICE_STATE_STARTING,
	BOW_DEVICE_STATE_SCANNING,
	BOW_DEVICE_STATE_CONNECTING,
	BOW_DEVICE_STATE_CONNECTED,
	BOW_DEVICE_STATE_NUM
} ENUM_BOW_DEVICE_STATE, *P_ENUM_BOW_DEVICE_STATE;

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

#endif /*_BOW_H */
