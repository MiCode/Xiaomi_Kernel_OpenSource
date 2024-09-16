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

#ifndef _P2P_STATE_H
#define _P2P_STATE_H

BOOLEAN
p2pStateInit_IDLE(IN P_ADAPTER_T prAdapter,
		  IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN P_BSS_INFO_T prP2pBssInfo, OUT P_ENUM_P2P_STATE_T peNextState);

VOID p2pStateAbort_IDLE(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN ENUM_P2P_STATE_T eNextState);

VOID p2pStateInit_SCAN(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo);

VOID p2pStateAbort_SCAN(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN ENUM_P2P_STATE_T eNextState);

VOID p2pStateInit_AP_CHANNEL_DETECT(IN P_ADAPTER_T prAdapter, IN P_P2P_FSM_INFO_T prP2pFsmInfo);

VOID
p2pStateAbort_AP_CHANNEL_DETECT(IN P_ADAPTER_T prAdapter,
				IN P_P2P_FSM_INFO_T prP2pFsmInfo,
				IN P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo, IN ENUM_P2P_STATE_T eNextState);

VOID
p2pStateInit_CHNL_ON_HAND(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo, IN P_P2P_FSM_INFO_T prP2pFsmInfo);

VOID
p2pStateAbort_CHNL_ON_HAND(IN P_ADAPTER_T prAdapter,
			   IN P_P2P_FSM_INFO_T prP2pFsmInfo,
			   IN P_BSS_INFO_T prP2pBssInfo, IN ENUM_P2P_STATE_T eNextState);

VOID
p2pStateAbort_REQING_CHANNEL(IN P_ADAPTER_T prAdapter,
			     IN P_P2P_FSM_INFO_T prP2pFsmInfo, IN ENUM_P2P_STATE_T eNextState);

VOID
p2pStateInit_GC_JOIN(IN P_ADAPTER_T prAdapter,
		     IN P_P2P_FSM_INFO_T prP2pFsmInfo,
		     IN P_BSS_INFO_T prP2pBssInfo, IN P_P2P_JOIN_INFO_T prJoinInfo, IN P_BSS_DESC_T prBssDesc);

VOID
p2pStateAbort_GC_JOIN(IN P_ADAPTER_T prAdapter,
		      IN P_P2P_FSM_INFO_T prP2pFsmInfo,
		      IN P_P2P_JOIN_INFO_T prJoinInfo, IN ENUM_P2P_STATE_T eNextState);

#endif
