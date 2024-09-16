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
** Id:
//Department/DaVinci/TRUNK/MT6620_5931_WiFi_Driver/os/linux/include/gl_p2p_os.h#28
*/

/*! \file   gl_p2p_os.h
*    \brief  List the external reference to OS for p2p GLUE Layer.
*
*    In this file we define the data structure - GLUE_INFO_T to store those objects
*    we acquired from OS - e.g. TIMER, SPINLOCK, NET DEVICE ... . And all the
*    external reference (header file, extern func() ..) to OS for GLUE Layer should
*    also list down here.
*/

#ifndef _GL_P2P_OS_H
#define _GL_P2P_OS_H

#define VENDOR_SPECIFIC_IE_LENGTH 300 /* 1(IE) + 1(length) + 0xff(max length) */
#define MAX_MULTI_P2P_IE_COUNT 4
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   V A R I A B L E
********************************************************************************
*/
#if CFG_ENABLE_WIFI_DIRECT && CFG_ENABLE_WIFI_DIRECT_CFG_80211
extern const struct net_device_ops p2p_netdev_ops;
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
extern struct net_device *g_P2pPrDev;
extern struct wireless_dev *gprP2pWdev;
extern struct wireless_dev *gprP2pRoleWdev[KAL_P2P_NUM];

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

struct _GL_P2P_INFO_T {

	/* P2P Device interface handle */
	/*only first p2p have this devhandler*/
	struct net_device *prDevHandler;
	/*struct net_device *prRoleDevHandler;*//* TH3 multiple P2P */

	struct net_device *aprRoleHandler;

	/* Todo : should move to the glueinfo or not*/
	/*UINT_8 ucRoleInterfaceNum;*//* TH3 multiple P2P */

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
	/* cfg80211 */
	struct wireless_dev *prWdev;
	/*struct wireless_dev *prRoleWdev[KAL_P2P_NUM];*//* TH3 multiple P2P */

	/*struct cfg80211_scan_request *prScanRequest;*//* TH3 multiple P2P */

	/*UINT_64 u8Cookie;*//* TH3 multiple P2P */

	/* Generation for station list update. */
	INT_32 i4Generation;

	/*UINT_32 u4OsMgmtFrameFilter;*//* TH3 multiple P2P */

#endif

	/* Device statistics */
	/*struct net_device_stats rNetDevStats;*//* TH3 multiple P2P */

	/* glue layer variables */
	/*move to glueinfo->adapter */
	/* BOOLEAN                     fgIsRegistered; */
	/*UINT_32 u4FreqInKHz;*//* TH3 multiple P2P */	/* frequency */
	UINT_8 ucRole;		/* 0: P2P Device, 1: Group Client, 2: Group Owner */
	/*UINT_8 ucIntent;*//* TH3 multiple P2P */	/* range: 0-15 */
	/*UINT_8 ucScanMode;*//* TH3 multiple P2P */	/* 0: Search & Listen, 1: Scan without probe response */

	/*ENUM_PARAM_MEDIA_STATE_T eState;*//* TH3 multiple P2P */
	/*UINT_32 u4PacketFilter;*//* TH3 multiple P2P */
	/*PARAM_MAC_ADDRESS aucMCAddrList[MAX_NUM_GROUP_ADDR];*//* TH3 multiple P2P */

	/* connection-requested peer information *//* TH3 multiple P2P */
	/*UINT_8 aucConnReqDevName[32];*//* TH3 multiple P2P */
	/*INT_32 u4ConnReqNameLength;*//* TH3 multiple P2P */
	/*PARAM_MAC_ADDRESS rConnReqPeerAddr;*//* TH3 multiple P2P */
	/*PARAM_MAC_ADDRESS rConnReqGroupAddr;*//* TH3 multiple P2P */	/* For invitation group. */
	/*UINT_8 ucConnReqDevType;*//* TH3 multiple P2P */
	/*INT_32 i4ConnReqConfigMethod;*//* TH3 multiple P2P */
	/*INT_32 i4ConnReqActiveConfigMethod;*//* TH3 multiple P2P */

	UINT_32 u4CipherPairwise;
	/*UINT_8 ucWSCRunning;*//* TH3 multiple P2P */

	/* 0: beacon, 1: probe req, 2: probe response, 3: assoc response */
	UINT_8 aucWSCIE[4][VENDOR_SPECIFIC_IE_LENGTH];
	UINT_16 u2WSCIELen[4];

	UINT_8 aucP2PIE[MAX_MULTI_P2P_IE_COUNT][VENDOR_SPECIFIC_IE_LENGTH];
	UINT_16 u2P2PIELen[MAX_MULTI_P2P_IE_COUNT];

#if CFG_SUPPORT_WFD
	UINT_8 aucWFDIE[VENDOR_SPECIFIC_IE_LENGTH];
	UINT_16 u2WFDIELen;
	/* UINT_8                      aucVenderIE[1024]; *//* Save the other IE for prove resp */
/* UINT_16                     u2VenderIELen; */
#endif

	/*UINT_8 ucOperatingChnl;*//* TH3 multiple P2P */
	/*UINT_8 ucInvitationType;*//* TH3 multiple P2P */

	/*UINT_32 u4InvStatus;*//* TH3 multiple P2P */

	/* For SET_STRUCT/GET_STRUCT */
	/*UINT_8 aucOidBuf[4096];*//* TH3 multiple P2P */

#if 1				/* CFG_SUPPORT_ANTI_PIRACY */
	/*UINT_8 aucSecCheck[256];*//* TH3 multiple P2P */
	/*UINT_8 aucSecCheckRsp[256];*//* TH3 multiple P2P */
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
	struct cfg80211_chan_def *chandef;
	UINT_32 cac_time_ms;
	BOOLEAN fgIsNetDevDetach;
#endif

#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
	/* Hotspot Client Management */
	/* dependent with  #define P2P_MAXIMUM_CLIENT_COUNT 10,
	 * fix me to PARAM_MAC_ADDRESS aucblackMACList[P2P_MAXIMUM_CLIENT_COUNT];
	 */
	PARAM_MAC_ADDRESS aucblackMACList[10];
	UINT_8 ucMaxClients;
#endif

#if CFG_SUPPORT_HOTSPOT_OPTIMIZATION
	/*BOOLEAN fgEnableHotspotOptimization;*//* TH3 multiple P2P */
	/*UINT_32 u4PsLevel;*//* TH3 multiple P2P */
#endif
};

struct _GL_P2P_DEV_INFO_T {
#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
	struct cfg80211_scan_request *prScanRequest;
	struct cfg80211_scan_request rBackupScanRequest;
	UINT_64 u8Cookie;
	UINT_32 u4OsMgmtFrameFilter;
#endif
	UINT_32 u4PacketFilter;
	PARAM_MAC_ADDRESS aucMCAddrList[MAX_NUM_GROUP_ADDR];
	UINT_8 ucWSCRunning;
};

#ifdef CONFIG_NL80211_TESTMODE
typedef struct _NL80211_DRIVER_TEST_PRE_PARAMS {
	UINT_16 idx_mode;
	UINT_16 idx;
	UINT_32 value;
} NL80211_DRIVER_TEST_PRE_PARAMS, *P_NL80211_DRIVER_TEST_PRE_PARAMS;

typedef struct _NL80211_DRIVER_TEST_PARAMS {
	UINT_32 index;
	UINT_32 buflen;
} NL80211_DRIVER_TEST_PARAMS, *P_NL80211_DRIVER_TEST_PARAMS;

/* P2P Sigma*/
typedef struct _NL80211_DRIVER_P2P_SIGMA_PARAMS {
	NL80211_DRIVER_TEST_PARAMS hdr;
	UINT_32 idx;
	UINT_32 value;
} NL80211_DRIVER_P2P_SIGMA_PARAMS, *P_NL80211_DRIVER_P2P_SIGMA_PARAMS;

/* Hotspot Client Management */
typedef struct _NL80211_DRIVER_hotspot_block_PARAMS {
	NL80211_DRIVER_TEST_PARAMS hdr;
	UINT_8 ucblocked;
	UINT_8 aucBssid[MAC_ADDR_LEN];
} NL80211_DRIVER_hotspot_block_PARAMS, *P_NL80211_DRIVER_hotspot_block_PARAMS;

#if CFG_SUPPORT_WFD
typedef struct _NL80211_DRIVER_WFD_PARAMS {
	NL80211_DRIVER_TEST_PARAMS hdr;
	UINT_32 WfdCmdType;
	UINT_8 WfdEnable;
	UINT_8 WfdCoupleSinkStatus;
	UINT_8 WfdSessionAvailable;
	UINT_8 WfdSigmaMode;
	UINT_16 WfdDevInfo;
	UINT_16 WfdControlPort;
	UINT_16 WfdMaximumTp;
	UINT_16 WfdExtendCap;
	UINT_8 WfdCoupleSinkAddress[MAC_ADDR_LEN];
	UINT_8 WfdAssociatedBssid[MAC_ADDR_LEN];
	UINT_8 WfdVideoIp[4];
	UINT_8 WfdAudioIp[4];
	UINT_16 WfdVideoPort;
	UINT_16 WfdAudioPort;
	UINT_32 WfdFlag;
	UINT_32 WfdPolicy;
	UINT_32 WfdState;
	UINT_8 WfdSessionInformationIE[24 * 8];	/* Include Subelement ID, length */
	UINT_16 WfdSessionInformationIELen;
	UINT_8 aucReserved1[2];
	UINT_8 aucWfdPrimarySinkMac[MAC_ADDR_LEN];
	UINT_8 aucWfdSecondarySinkMac[MAC_ADDR_LEN];
	UINT_32 WfdAdvanceFlag;
	/* Group 1 64 bytes */
	UINT_8 aucWfdLocalIp[4];
	UINT_16 WfdLifetimeAc2;	/* Unit is 2 TU */
	UINT_16 WfdLifetimeAc3;	/* Unit is 2 TU */
	UINT_16 WfdCounterThreshold;	/* Unit is ms */
	UINT_8 aucReserved2[54];
	/* Group 3 64 bytes */
	UINT_8 aucReserved3[64];
	/* Group 3 64 bytes */
	UINT_8 aucReserved4[64];
} NL80211_DRIVER_WFD_PARAMS, *P_NL80211_DRIVER_WFD_PARAMS;
#endif
#endif

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

BOOLEAN p2pRegisterToWlan(P_GLUE_INFO_T prGlueInfo);

BOOLEAN p2pUnregisterToWlan(P_GLUE_INFO_T prGlueInfo);

BOOLEAN p2pLaunch(P_GLUE_INFO_T prGlueInfo);

BOOLEAN p2pRemove(P_GLUE_INFO_T prGlueInfo);

VOID p2pSetMode(IN UINT_8 ucAPMode);

BOOLEAN glRegisterP2P(P_GLUE_INFO_T prGlueInfo, const char *prDevName, const char *prDevName2, UINT_8 ucApMode);

BOOLEAN glUnregisterP2P(P_GLUE_INFO_T prGlueInfo);

BOOLEAN p2pNetRegister(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgIsRtnlLockAcquired);

BOOLEAN p2pNetUnregister(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgIsRtnlLockAcquired);

BOOLEAN p2PFreeInfo(P_GLUE_INFO_T prGlueInfo);

VOID p2pSetSuspendMode(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgEnable);
BOOLEAN glP2pCreateWirelessDevice(P_GLUE_INFO_T prGlueInfo);
VOID glP2pDestroyWirelessDevice(VOID);
VOID p2pUpdateChannelTableByDomain(P_GLUE_INFO_T prGlueInfo);
#endif
