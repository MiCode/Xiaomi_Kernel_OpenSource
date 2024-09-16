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

#ifndef _P2P_DEV_STATE_H
#define _P2P_DEV_STATE_H

BOOLEAN
p2pDevStateInit_IDLE(IN P_ADAPTER_T prAdapter,
		     IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo, OUT P_ENUM_P2P_DEV_STATE_T peNextState);

VOID p2pDevStateAbort_IDLE(IN P_ADAPTER_T prAdapter);

BOOLEAN
p2pDevStateInit_REQING_CHANNEL(IN P_ADAPTER_T prAdapter,
			       IN UINT_8 ucBssIdx,
			       IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo, OUT P_ENUM_P2P_DEV_STATE_T peNextState);

VOID
p2pDevStateAbort_REQING_CHANNEL(IN P_ADAPTER_T prAdapter,
				IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo, IN ENUM_P2P_DEV_STATE_T eNextState);

VOID
p2pDevStateInit_CHNL_ON_HAND(IN P_ADAPTER_T prAdapter,
			     IN P_BSS_INFO_T prP2pBssInfo,
			     IN P_P2P_DEV_FSM_INFO_T prP2pDevFsmInfo, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo);

VOID
p2pDevStateAbort_CHNL_ON_HAND(IN P_ADAPTER_T prAdapter,
			      IN P_BSS_INFO_T prP2pBssInfo,
			      IN P_P2P_DEV_FSM_INFO_T prP2pDevFsmInfo,
			IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo,
			IN ENUM_P2P_DEV_STATE_T eNextState);

VOID p2pDevStateInit_SCAN(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_P2P_SCAN_REQ_INFO_T prScanReqInfo);

VOID p2pDevStateAbort_SCAN(IN P_ADAPTER_T prAdapter, IN P_P2P_DEV_FSM_INFO_T prP2pDevFsmInfo);

BOOLEAN
p2pDevStateInit_OFF_CHNL_TX(IN P_ADAPTER_T prAdapter,
			    IN P_P2P_DEV_FSM_INFO_T prP2pDevFsmInfo,
			    IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo,
			    IN P_P2P_MGMT_TX_REQ_INFO_T prP2pMgmtTxInfo, OUT P_ENUM_P2P_DEV_STATE_T peNextState);

VOID
p2pDevStateAbort_OFF_CHNL_TX(IN P_ADAPTER_T prAdapter,
			     IN P_P2P_MGMT_TX_REQ_INFO_T prP2pMgmtTxInfo,
			     IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo, IN ENUM_P2P_DEV_STATE_T eNextState);

#endif
