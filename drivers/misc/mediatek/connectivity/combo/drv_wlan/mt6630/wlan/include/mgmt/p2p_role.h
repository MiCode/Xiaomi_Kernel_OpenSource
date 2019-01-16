#ifndef _P2P_ROLE_H
#define _P2P_ROLE_H


typedef enum _ENUM_BUFFER_TYPE_T {
	ENUM_FRAME_TYPE_EXTRA_IE_BEACON,
	ENUM_FRAME_TYPE_EXTRA_IE_ASSOC_RSP,
	ENUM_FRAME_TYPE_EXTRA_IE_PROBE_RSP,
	ENUM_FRAME_TYPE_PROBE_RSP_TEMPLATE,
	ENUM_FRAME_TYPE_BEACON_TEMPLATE,
	ENUM_FRAME_IE_NUM
} ENUM_BUFFER_TYPE_T, *P_ENUM_BUFFER_TYPE_T;

typedef enum _ENUM_HIDDEN_SSID_TYPE_T {
	ENUM_HIDDEN_SSID_NONE,
	ENUM_HIDDEN_SSID_LEN,
	ENUM_HIDDEN_SSID_ZERO_CONTENT,
	ENUM_HIDDEN_SSID_NUM
} ENUM_HIDDEN_SSID_TYPE_T, *P_ENUM_HIDDEN_SSID_TYPE_T;


typedef struct _P2P_BEACON_UPDATE_INFO_T {
	PUINT_8 pucBcnHdr;
	UINT_32 u4BcnHdrLen;
	PUINT_8 pucBcnBody;
	UINT_32 u4BcnBodyLen;
} P2P_BEACON_UPDATE_INFO_T, *P_P2P_BEACON_UPDATE_INFO_T;

typedef struct _P2P_PROBE_RSP_UPDATE_INFO_T {
	P_MSDU_INFO_T prProbeRspMsduTemplate;
} P2P_PROBE_RSP_UPDATE_INFO_T, *P_P2P_PROBE_RSP_UPDATE_INFO_T;

typedef struct _P2P_ASSOC_RSP_UPDATE_INFO_T {
	PUINT_8 pucAssocRspExtIE;
	UINT_16 u2AssocIELen;
} P2P_ASSOC_RSP_UPDATE_INFO_T, *P_P2P_ASSOC_RSP_UPDATE_INFO_T;


typedef struct _AP_CRYPTO_SETTINGS_T {
	UINT_32 u4WpaVersion;
	UINT_32 u4CipherGroup;
	INT_32	i4NumOfCiphers;
	UINT_32 aucCiphersPairwise[5];
	INT_32	i4NumOfAkmSuites;
	UINT_32 aucAkmSuites[2];
	BOOLEAN fgIsControlPort;
	UINT_16 u2ControlPortBE;
	BOOLEAN fgIsControlPortEncrypt;
} AP_CRYPTO_SETTINGS_T, *P_AP_CRYPTO_SETTINGS_T;

/* ////////////////////////////////// Message //////////////////////////////////////////////////// */

typedef struct _MSG_P2P_BEACON_UPDATE_T {
	MSG_HDR_T	rMsgHdr;
	UINT_8		ucRoleIndex;
	UINT_32		u4BcnHdrLen;
	UINT_32		u4BcnBodyLen;
	PUINT_8		pucBcnHdr;
	PUINT_8		pucBcnBody;
	UINT_8		aucBuffer[1];	/* Header & Body are put here. */
} MSG_P2P_BEACON_UPDATE_T, *P_MSG_P2P_BEACON_UPDATE_T;

typedef struct _MSG_P2P_MGMT_FRAME_UPDATE_T {
	MSG_HDR_T		rMsgHdr;
	ENUM_BUFFER_TYPE_T	eBufferType;
	UINT_32			u4BufferLen;
	UINT_8			aucBuffer[1];
} MSG_P2P_MGMT_FRAME_UPDATE_T, *P_MSG_P2P_MGMT_FRAME_UPDATE_T;


typedef struct _MSG_P2P_SWITCH_OP_MODE_T {
	MSG_HDR_T	rMsgHdr;/* Must be the first member */
	ENUM_OP_MODE_T	eOpMode;
	UINT_8		ucRoleIdx;
} MSG_P2P_SWITCH_OP_MODE_T, *P_MSG_P2P_SWITCH_OP_MODE_T;

typedef struct _MSG_P2P_MGMT_FRAME_REGISTER_T {
	MSG_HDR_T	rMsgHdr;/* Must be the first member */
	UINT_16		u2FrameType;
	BOOLEAN		fgIsRegister;
} MSG_P2P_MGMT_FRAME_REGISTER_T, *P_MSG_P2P_MGMT_FRAME_REGISTER_T;

typedef struct _MSG_P2P_CHNL_ABORT_T {
	MSG_HDR_T	rMsgHdr;/* Must be the first member */
	UINT_64		u8Cookie;
} MSG_P2P_CHNL_ABORT_T, *P_MSG_P2P_CHNL_ABORT_T;


typedef struct _MSG_P2P_CONNECTION_REQUEST_T {
	MSG_HDR_T		rMsgHdr;/* Must be the first member */
	UINT_8			ucRoleIdx;
	P2P_SSID_STRUCT_T	rSsid;
	UINT_8			aucBssid[MAC_ADDR_LEN];
	ENUM_CHNL_EXT_T		eChnlSco;
	RF_CHANNEL_INFO_T	rChannelInfo;
	UINT_32			u4IELen;
	UINT_8			aucIEBuf[1];
	/* TODO: Auth Type, OPEN, SHARED, FT, EAP... */
} MSG_P2P_CONNECTION_REQUEST_T, *P_MSG_P2P_CONNECTION_REQUEST_T;


typedef struct _MSG_P2P_CONNECTION_ABORT_T {
	MSG_HDR_T	rMsgHdr;/* Must be the first member. */
	UINT_8		ucRoleIdx;
	UINT_8		aucTargetID[MAC_ADDR_LEN];
	UINT_16		u2ReasonCode;
	BOOLEAN		fgSendDeauth;
} MSG_P2P_CONNECTION_ABORT_T, *P_MSG_P2P_CONNECTION_ABORT_T;


typedef struct _MSG_P2P_START_AP_T {
	MSG_HDR_T		rMsgHdr;
	UINT_32			u4DtimPeriod;
	UINT_32			u4BcnInterval;
	UINT_8			aucSsid[32];
	UINT_16			u2SsidLen;
	UINT_8			ucHiddenSsidType;
	BOOLEAN			fgIsPrivacy;
	UINT_8			ucRoleIdx;
	AP_CRYPTO_SETTINGS_T	rEncryptionSettings;
	INT_32			i4InactiveTimeout;
} MSG_P2P_START_AP_T, *P_MSG_P2P_START_AP_T;


typedef struct _P2P_STATION_INFO_T {
	UINT_32 u4InactiveTime;
	UINT_32 u4RxBytes;	/* TODO: */
	UINT_32 u4TxBytes;	/* TODO: */
	UINT_32 u4RxPackets;	/* TODO: */
	UINT_32 u4TxPackets;	/* TODO: */
	/* TODO: Add more for requirement. */
} P2P_STATION_INFO_T, *P_P2P_STATION_INFO_T;



/* 3  --------------- WFA P2P Attributes Handler prototype --------------- */
typedef UINT_32 (*PFN_APPEND_ATTRI_FUNC) (P_ADAPTER_T, BOOLEAN, PUINT_16, PUINT_8, UINT_16);

typedef UINT_32 (*PFN_CALCULATE_VAR_ATTRI_LEN_FUNC) (P_ADAPTER_T, P_STA_RECORD_T);


typedef struct _APPEND_VAR_ATTRI_ENTRY_T {
	UINT_16					u2EstimatedFixedAttriLen;	/* For fixed length */
	PFN_CALCULATE_VAR_ATTRI_LEN_FUNC	pfnCalculateVariableAttriLen;
	PFN_APPEND_ATTRI_FUNC			pfnAppendAttri;
} APPEND_VAR_ATTRI_ENTRY_T, *P_APPEND_VAR_ATTRI_ENTRY_T;

/* //////////////////////////////////////////////////////////////// */


typedef enum _ENUM_P2P_ROLE_STATE_T {
	P2P_ROLE_STATE_IDLE = 0,
	P2P_ROLE_STATE_SCAN,
	P2P_ROLE_STATE_REQING_CHANNEL,
	P2P_ROLE_STATE_AP_CHNL_DETECTION,	/* Requesting Channel to Send Specific Frame. */
	P2P_ROLE_STATE_GC_JOIN,
	P2P_ROLE_STATE_NUM
} ENUM_P2P_ROLE_STATE_T, *P_ENUM_P2P_ROLE_STATE_T;

typedef enum _ENUM_P2P_CONNECTION_TYPE_T {
	P2P_CONNECTION_TYPE_IDLE = 0,
	P2P_CONNECTION_TYPE_GO,
	P2P_CONNECTION_TYPE_GC,
	P2P_CONNECTION_TYPE_PURE_AP,
	P2P_CONNECTION_TYPE_NUM
} ENUM_P2P_CONNECTION_TYPE_T, *P_ENUM_P2P_CONNECTION_TYPE_T;

typedef struct _P2P_JOIN_INFO_T {
	UINT_32		ucSeqNumOfReqMsg;
	UINT_8		ucAvailableAuthTypes;
	P_STA_RECORD_T	prTargetStaRec;
	P_BSS_DESC_T	prTargetBssDesc;
	BOOLEAN		fgIsJoinComplete;
	/* For ASSOC Rsp. */
	UINT_32		u4BufLength;
	UINT_8		aucIEBuf[MAX_IE_LENGTH];
} P2P_JOIN_INFO_T, *P_P2P_JOIN_INFO_T;

/* For STA & AP mode. */
typedef struct _P2P_CONNECTION_REQ_INFO_T {
	ENUM_P2P_CONNECTION_TYPE_T	eConnRequest;
	P2P_SSID_STRUCT_T		rSsidStruct;
	UINT_8				aucBssid[MAC_ADDR_LEN];

	/* AP preferred channel. */
	RF_CHANNEL_INFO_T		rChannelInfo;
	ENUM_CHNL_EXT_T			eChnlExt;

	/* For ASSOC Req. */
	UINT_32				u4BufLength;
	UINT_8				aucIEBuf[MAX_IE_LENGTH];
} P2P_CONNECTION_REQ_INFO_T, *P_P2P_CONNECTION_REQ_INFO_T;


#define P2P_ROLE_INDEX_2_ROLE_FSM_INFO(_prAdapter, _RoleIndex) \
	((_prAdapter)->rWifiVar.aprP2pRoleFsmInfo[_RoleIndex])

struct _P2P_ROLE_FSM_INFO_T {
	UINT_8				ucRoleIndex;

	UINT_8				ucBssIndex;

	/* State related. */
	ENUM_P2P_ROLE_STATE_T		eCurrentState;

	/* Channel related. */
	P2P_CHNL_REQ_INFO_T		rChnlReqInfo;

	/* Scan related. */
	P2P_SCAN_REQ_INFO_T		rScanReqInfo;

	/* FSM Timer */
	TIMER_T				rP2pRoleFsmTimeoutTimer;

	/* Packet filter for P2P module. */
	UINT_32				u4P2pPacketFilter;

	/* GC Join related. */
	P2P_JOIN_INFO_T			rJoinInfo;

	/* Connection related. */
	P2P_CONNECTION_REQ_INFO_T	rConnReqInfo;

	/* Beacon Information. */
	P2P_BEACON_UPDATE_INFO_T	rBeaconUpdateInfo;
};

/*========================= Initial ============================*/


UINT_8 p2pRoleFsmInit(IN P_ADAPTER_T prAdapter, IN UINT_8 ucRoleIdx);


VOID p2pRoleFsmUninit(IN P_ADAPTER_T prAdapter, IN UINT_8 ucRoleIdx);

/*================== Message Event ==================*/

VOID p2pRoleFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo);


VOID p2pRoleFsmRunEventStartAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);


VOID p2pRoleFsmRunEventStopAP(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventScanRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);


VOID
p2pRoleFsmRunEventScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr, IN P_P2P_ROLE_FSM_INFO_T
			   prP2pRoleFsmInfo);



VOID p2pRoleFsmRunEventJoinComplete(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);



VOID p2pRoleFsmRunEventBeaconTimeout(IN P_ADAPTER_T prAdapter, IN P_BSS_INFO_T prP2pBssInfo);

WLAN_STATUS
p2pRoleFsmRunEventAAAComplete(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prP2pBssInfo);


WLAN_STATUS
p2pRoleFsmRunEventAAASuccess(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prP2pBssInfo);

VOID
p2pRoleFsmRunEventAAATxFail(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_BSS_INFO_T prP2pBssInfo);

VOID p2pRoleFsmRunEventConnectionRequest(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventConnectionAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);


VOID
p2pRoleFsmRunEventChnlGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr, IN P_P2P_ROLE_FSM_INFO_T
			    prP2pRoleFsmInfo);

WLAN_STATUS
p2pRoleFsmRunEventDeauthTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T
			       rTxDoneStatus);


VOID
p2pRoleFsmRunEventRxDeauthentication(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb);

VOID
p2pRoleFsmRunEventRxDisassociation(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prSwRfb);

/* //////////////////////// TO BE REFINE ///////////////////// */
VOID p2pRoleFsmRunEventSwitchOPMode(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pFsmRunEventBeaconUpdate(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID p2pRoleFsmRunEventDissolve(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);


VOID
p2pProcessEvent_UpdateNOAParam(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN P_EVENT_UPDATE_NOA_PARAMS_T
			       prEventUpdateNoaParam);


#endif
