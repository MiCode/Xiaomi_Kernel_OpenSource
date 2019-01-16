#ifndef _P2P_ROLE_STATE_H
#define _P2P_ROLE_STATE_H




VOID
p2pRoleStateInit_IDLE(IN P_ADAPTER_T prAdapter,
		      IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo, IN P_BSS_INFO_T prP2pBssInfo);

VOID
p2pRoleStateAbort_IDLE(IN P_ADAPTER_T prAdapter,
		       IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo,
		       IN P_P2P_CHNL_REQ_INFO_T prP2pChnlReqInfo);

VOID
p2pRoleStateInit_SCAN(IN P_ADAPTER_T prAdapter,
		      IN UINT_8 ucBssIndex, IN P_P2P_SCAN_REQ_INFO_T prScanReqInfo);

VOID p2pRoleStateAbort_SCAN(IN P_ADAPTER_T prAdapter, IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo);

VOID
p2pRoleStateInit_REQING_CHANNEL(IN P_ADAPTER_T prAdapter,
				IN UINT_8 ucBssIdx, IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo);

VOID
p2pRoleStateAbort_REQING_CHANNEL(IN P_ADAPTER_T prAdapter,
				 IN P_BSS_INFO_T prP2pRoleBssInfo,
				 IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo,
				 IN ENUM_P2P_ROLE_STATE_T eNextState);

VOID
p2pRoleStateInit_AP_CHNL_DETECTION(IN P_ADAPTER_T prAdapter,
				   IN UINT_8 ucBssIndex,
				   IN P_P2P_SCAN_REQ_INFO_T prScanReqInfo,
				   IN P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo);

VOID
p2pRoleStateAbort_AP_CHNL_DETECTION(IN P_ADAPTER_T prAdapter,
				    IN UINT_8 ucBssIndex,
				    IN P_P2P_CONNECTION_REQ_INFO_T prP2pConnReqInfo,
				    IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo,
				    IN P_P2P_SCAN_REQ_INFO_T prP2pScanReqInfo,
				    IN ENUM_P2P_ROLE_STATE_T eNextState);

VOID
p2pRoleStateInit_GC_JOIN(IN P_ADAPTER_T prAdapter,
			 IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo,
			 IN P_P2P_CHNL_REQ_INFO_T prChnlReqInfo);

VOID
p2pRoleStateAbort_GC_JOIN(IN P_ADAPTER_T prAdapter,
			  IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo,
			  IN P_P2P_JOIN_INFO_T prJoinInfo, IN ENUM_P2P_ROLE_STATE_T eNextState);

VOID
p2pRoleStatePrepare_To_REQING_CHANNEL_STATE(IN P_ADAPTER_T prAdapter,
                        IN P_BSS_INFO_T prBssInfo,
					    IN P_P2P_CONNECTION_REQ_INFO_T prConnReqInfo,
					    OUT P_P2P_CHNL_REQ_INFO_T prChnlReqInfo);

#endif
