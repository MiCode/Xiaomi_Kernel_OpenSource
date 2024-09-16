/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _FWCFG_H
#define _FWCFG_H
#include "precomp.h"
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
#ifdef FW_CFG_SUPPORT
#define MAX_CMD_ITEM_MAX		4
#define MAX_CMD_NAME_MAX_LENGTH		32
#define MAX_CMD_VALUE_MAX_LENGTH	32

#define MAX_CMD_TYPE_LENGTH		1
#define MAX_CMD_RESERVE_LENGTH		1
#define MAX_CMD_STRING_LENGTH		1
#define MAX_CMD_VALUE_LENGTH		1

#define CMD_FORMAT_V1_LENGTH		(MAX_CMD_NAME_MAX_LENGTH + \
					MAX_CMD_VALUE_MAX_LENGTH + MAX_CMD_TYPE_LENGTH + \
					MAX_CMD_STRING_LENGTH + MAX_CMD_VALUE_LENGTH + MAX_CMD_RESERVE_LENGTH)

#define MAX_CMD_BUFFER_LENGTH		(CMD_FORMAT_V1_LENGTH * MAX_CMD_ITEM_MAX)

#define FW_CFG_FILE "/vendor/firmware/wifi_fw.cfg"
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

enum _CMD_VER_T {
	CMD_VER_1,
	CMD_VER_1_EXT
};

enum _CMD_TYPE_T {
	CMD_TYPE_QUERY,
	CMD_TYPE_SET
};

struct _CMD_FORMAT_V1_T {
	UINT_8 itemType;
	UINT_8 itemStringLength;
	UINT_8 itemValueLength;
	UINT_8 Reserved;
	UINT_8 itemString[MAX_CMD_NAME_MAX_LENGTH];
	UINT_8 itemValue[MAX_CMD_VALUE_MAX_LENGTH];
};

struct _CMD_HEADER_T {
	enum _CMD_VER_T cmdVersion;
	enum _CMD_TYPE_T cmdType;
	UINT_8 itemNum;
	UINT_16 cmdBufferLen;
	UINT_8 buffer[MAX_CMD_BUFFER_LENGTH];
};

struct WLAN_CFG_PARSE_STATE_S {
	CHAR *ptr;
	CHAR *text;
	INT_32 nexttoken;
	UINT_32 maxSize;
};

struct _FW_CFG {
	PUINT_8 key;
	PUINT_8 value;
};
#ifdef CFG_SUPPORT_COEX_IOT_AP
#define COEX_ISSUE_TYPE_ID    1
struct FwCfgForIotAP {
	UINT_8 ucIotType;
	UINT_8 aucIotApMacAddr[MAC_ADDR_LEN];
	UINT_8 ucMacAddrMask[MAC_ADDR_LEN];
	PUINT_8 aucEnableCmdString;
	PUINT_8 aucDisableCmdString;
};
#endif
/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

INT_32 getFwCfgItemNum(void);

PUINT_8 getFwCfgItemKey(UINT_8 i);

PUINT_8 getFwCfgItemValue(UINT_8 i);

void wlanCfgFwSetParam(PUINT_8 fwBuffer, PCHAR cmdStr, PCHAR value, int num, int type);

WLAN_STATUS wlanCfgSetGetFw(IN P_ADAPTER_T prAdapter, const PCHAR fwBuffer, int cmdNum, enum _CMD_TYPE_T cmdType);

WLAN_STATUS wlanFwCfgParse(IN P_ADAPTER_T prAdapter, PUINT_8 pucConfigBuf);

WLAN_STATUS wlanFwArrayCfg(IN P_ADAPTER_T prAdpter);

WLAN_STATUS wlanFwFileCfg(IN P_ADAPTER_T prAdpter);
#ifdef CFG_SUPPORT_COEX_IOT_AP
WLAN_STATUS wlanFWCfgForIotAP(IN P_ADAPTER_T prAdpter, UINT_8 aucBssid[]);
WLAN_STATUS wlanFWCfgForceDisIotAP(IN P_ADAPTER_T prAdapter);
#endif

#endif
#endif
