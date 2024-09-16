/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic/bow.h#1
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
struct BT_OVER_WIFI_COMMAND_HEADER {
	uint8_t ucCommandId;
	uint8_t ucSeqNumber;
	uint16_t u2PayloadLength;
};

struct BT_OVER_WIFI_COMMAND {
	struct BT_OVER_WIFI_COMMAND_HEADER rHeader;
	uint8_t aucPayload[0];
};

struct BT_OVER_WIFI_EVENT_HEADER {
	uint8_t ucEventId;
	uint8_t ucSeqNumber;
	uint16_t u2PayloadLength;
};

struct BT_OVER_WIFI_EVENT {
	struct BT_OVER_WIFI_EVENT_HEADER rHeader;
	uint8_t aucPayload[0];
};

struct CHANNEL_DESC {
	uint8_t ucChannelBand;
	uint8_t ucChannelNum;
};

/* Command Structures */
struct BOW_SETUP_CONNECTION {
/* Fixed to 2.4G */
	uint8_t ucChannelNum;
	uint8_t ucReserved1;
	uint8_t aucPeerAddress[6];
	uint16_t u2BeaconInterval;
	uint8_t ucTimeoutDiscovery;
	uint8_t ucTimeoutInactivity;
	uint8_t ucRole;
	uint8_t ucPAL_Capabilities;
	int8_t cMaxTxPower;
	uint8_t ucReserved2;

/* Pending, for future BOW 5G supporting. */
/*    UINT_8          aucPeerAddress[6];
 *    UINT_16         u2BeaconInterval;
 *   UINT_8          ucTimeoutDiscovery;
 *   UINT_8          ucTimeoutInactivity;
 *   UINT_8          ucRole;
 *   UINT_8          ucPAL_Capabilities;
 *   INT_8           cMaxTxPower;
 *   UINT_8          ucChannelListNum;
 *   CHANNEL_DESC    arChannelList[1];
 */
};

struct BOW_DESTROY_CONNECTION {
	uint8_t aucPeerAddress[6];
	uint8_t aucReserved[2];
};

struct BOW_SET_PTK {
	uint8_t aucPeerAddress[6];
	uint8_t aucReserved[2];
	uint8_t aucTemporalKey[16];
};

struct BOW_READ_RSSI {
	uint8_t aucPeerAddress[6];
	uint8_t aucReserved[2];
};

struct BOW_READ_LINK_QUALITY {
	uint8_t aucPeerAddress[6];
	uint8_t aucReserved[2];
};

struct BOW_SHORT_RANGE_MODE {
	uint8_t aucPeerAddress[6];
	int8_t cTxPower;
	uint8_t ucReserved;
};

/* Event Structures */
struct BOW_COMMAND_STATUS {
	uint8_t ucStatus;
	uint8_t ucReserved[3];
};

struct BOW_MAC_STATUS {
	uint8_t aucMacAddr[6];
	uint8_t ucAvailability;
	uint8_t ucNumOfChannel;
	struct CHANNEL_DESC arChannelList[MAX_BOW_NUMBER_OF_CHANNEL];
};

struct BOW_LINK_CONNECTED {
	struct CHANNEL_DESC rChannel;
	uint8_t aucReserved;
	uint8_t aucPeerAddress[6];
};

struct BOW_LINK_DISCONNECTED {
	uint8_t ucReason;
	uint8_t aucReserved;
	uint8_t aucPeerAddress[6];
};

struct BOW_RSSI {
	int8_t cRssi;
	uint8_t aucReserved[3];
};

struct BOW_LINK_QUALITY {
	uint8_t ucLinkQuality;
	uint8_t aucReserved[3];
};

enum ENUM_BOW_CMD_ID {
	BOW_CMD_ID_GET_MAC_STATUS = 1,
	BOW_CMD_ID_SETUP_CONNECTION,
	BOW_CMD_ID_DESTROY_CONNECTION,
	BOW_CMD_ID_SET_PTK,
	BOW_CMD_ID_READ_RSSI,
	BOW_CMD_ID_READ_LINK_QUALITY,
	BOW_CMD_ID_SHORT_RANGE_MODE,
	BOW_CMD_ID_GET_CHANNEL_LIST,
};

enum ENUM_BOW_EVENT_ID {
	BOW_EVENT_ID_COMMAND_STATUS = 1,
	BOW_EVENT_ID_MAC_STATUS,
	BOW_EVENT_ID_LINK_CONNECTED,
	BOW_EVENT_ID_LINK_DISCONNECTED,
	BOW_EVENT_ID_RSSI,
	BOW_EVENT_ID_LINK_QUALITY,
	BOW_EVENT_ID_CHANNEL_LIST,
	BOW_EVENT_ID_CHANNEL_SELECTED,
};

enum ENUM_BOW_DEVICE_STATE {
	BOW_DEVICE_STATE_DISCONNECTED = 0,
	BOW_DEVICE_STATE_DISCONNECTING,
	BOW_DEVICE_STATE_ACQUIRING_CHANNEL,
	BOW_DEVICE_STATE_STARTING,
	BOW_DEVICE_STATE_SCANNING,
	BOW_DEVICE_STATE_CONNECTING,
	BOW_DEVICE_STATE_CONNECTED,
	BOW_DEVICE_STATE_NUM
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

#endif /*_BOW_H */
