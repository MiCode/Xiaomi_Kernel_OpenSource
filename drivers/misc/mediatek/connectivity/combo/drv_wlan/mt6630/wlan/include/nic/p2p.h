/*
** $Id: //Department/DaVinci/TRUNK/WiFi_P2P_Driver/include/nic/p2p.h#3 $
*/



/*
** $Log: p2p.h $
**
** 07 25 2014 eason.tsai
** AOSP
**
** 10 08 2013 yuche.tsai
** [ALPS01065606] [Volunteer Patch][MT6630][Wi-Fi Direct][Driver] MT6630 Wi-Fi Direct Driver Patch
** Update Wi-Fi Direct Source.
**
** 08 28 2013 yuche.tsai
** [BORA00002761] [MT6630][Wi-Fi Direct][Driver] Group Interface formation
** Fix Wi-Fi Direct channel width & RX channel indication issue.
**
** 08 22 2013 yuche.tsai
** [BORA00002761] [MT6630][Wi-Fi Direct][Driver] Group Interface formation
** [BORA00000779] [MT6620] Emulation For TX Code Check In
**	Make P2P group interface formation success.
**
** 08 13 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Update driver for P2P scan & listen.
**
** 07 19 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Code update for P2P.
**
** 02 27 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Add p2p_rlm.c, p2p_rlm_obss.c, fix compile warning & error.
**
** 02 27 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Add new code, fix compile warning.
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
*
* 07 17 2012 yuche.tsai
* NULL
* Compile no error before trial run.
*
* 10 20 2010 wh.su
* [WCXRP00000124] [MT6620 Wi-Fi] [Driver] Support the dissolve P2P Group
* Add the code to support disconnect p2p group
*
* 09 21 2010 kevin.huang
* [WCXRP00000054] [MT6620 Wi-Fi][Driver] Restructure driver for second Interface
* Isolate P2P related function for Hardware Software Bundle
*
* 08 03 2010 cp.wu
* NULL
* [Wi-Fi Direct] add framework for driver hooks
*
* 07 08 2010 cp.wu
*
* [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
*
* 06 23 2010 cp.wu
* [WPD00003833][MT6620 and MT5931] Driver migration
* p2p interface revised to be sync. with HAL
*
* 06 06 2010 kevin.huang
* [WPD00003832][MT6620 5931] Create driver base
* [MT6620 5931] Create driver base
*
* 05 18 2010 cp.wu
* [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
* add parameter to control:
* 1) auto group owner
* 2) P2P-PS parameter (CTWindow, NoA descriptors)
*
* 05 18 2010 cp.wu
* [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
* correct WPS Device Password ID definition.
*
* 05 17 2010 cp.wu
* [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
* implement get scan result.
*
* 05 17 2010 cp.wu
* [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
* add basic handling framework for wireless extension ioctls.
*
* 05 14 2010 cp.wu
* [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
* add ioctl framework for Wi-Fi Direct by reusing wireless extension ioctls as well
*
* 05 11 2010 cp.wu
* [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
* p2p ioctls revised.
*
* 05 10 2010 cp.wu
* [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
* implement basic wi-fi direct framework
*
* 05 07 2010 cp.wu
* [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
* add basic framework for implementating P2P driver hook.
*
*
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

/* Device Capability Definition. */
#define P2P_MAXIMUM_CLIENT_COUNT                    10
#define P2P_MAXIMUM_NOA_COUNT                       8

#define P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE            51	/* Contains 6 sub-band. */

/* Memory Size Definition. */
#define P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE           768
#define WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE           300

#define P2P_WILDCARD_SSID           "DIRECT-"

/* Device Charactoristic. */
#define P2P_AP_CHNL_HOLD_TIME_MS 6000
#define P2P_DEFAULT_LISTEN_CHANNEL                   1

/*******************************************************************************
 *                                 M A C R O S
 ********************************************************************************
 */

#if DBG
#define ASSERT_BREAK(_exp) \
	{ \
		if (!(_exp)) { \
			ASSERT(FALSE); \
			break; \
		} \
	}

#else
#define ASSERT_BREAK(_exp)
#endif


#define p2pChangeMediaState(_prAdapter, _prP2pBssInfo, _eNewMediaState) \
	(_prP2pBssInfo->eConnectionState = (_eNewMediaState));

/*******************************************************************************
 *                             D A T A   T Y P E S
 ********************************************************************************
 */

struct _P2P_INFO_T {
	UINT_32				u4DeviceNum;
	EVENT_P2P_DEV_DISCOVER_RESULT_T arP2pDiscoverResult[CFG_MAX_NUM_BSS_LIST];
	PUINT_8				pucCurrIePtr;
	UINT_8				aucCommIePool[CFG_MAX_COMMON_IE_BUF_LEN];	/* A common pool for IE of all
											 *scan results. */
};

typedef enum {
	ENUM_P2P_PEER_GROUP,
	ENUM_P2P_PEER_DEVICE,
	ENUM_P2P_PEER_NUM
} ENUM_P2P_PEER_TYPE, *P_ENUM_P2P_PEER_TYPE;

typedef struct _P2P_DEVICE_INFO {
	UINT_8	aucDevAddr[PARAM_MAC_ADDR_LEN];
	UINT_8	aucIfAddr[PARAM_MAC_ADDR_LEN];
	UINT_8	ucDevCapabilityBitmap;
	INT_32	i4ConfigMethod;
	UINT_8	aucPrimaryDeviceType[8];
	UINT_8	aucSecondaryDeviceType[8];
	UINT_8	aucDeviceName[P2P_DEVICE_NAME_LENGTH];
} P2P_DEVICE_INFO, *P_P2P_DEVICE_INFO;

typedef struct _P2P_GROUP_INFO {
	PARAM_SSID_T	rGroupID;
	P2P_DEVICE_INFO rGroupOwnerInfo;
	UINT_8		ucMemberNum;
	P2P_DEVICE_INFO arMemberInfo[P2P_MEMBER_NUM];
} P2P_GROUP_INFO, *P_P2P_GROUP_INFO;

typedef struct _P2P_NETWORK_INFO {
	ENUM_P2P_PEER_TYPE eNodeType;

	union {
		P2P_GROUP_INFO	rGroupInfo;
		P2P_DEVICE_INFO rDeviceInfo;
	} node;
} P2P_NETWORK_INFO, *P_P2P_NETWORK_INFO;

typedef struct _P2P_NETWORK_LIST {
	UINT_8			ucNetworkNum;
	P2P_NETWORK_INFO	rP2PNetworkInfo[P2P_NETWORK_NUM];
} P2P_NETWORK_LIST, *P_P2P_NETWORK_LIST;

typedef struct _P2P_DISCONNECT_INFO {
	UINT_8	ucRole;
	UINT_8	ucRsv[3];
} P2P_DISCONNECT_INFO, *P_P2P_DISCONNECT_INFO;


typedef struct _P2P_SSID_STRUCT_T {
	UINT_8	aucSsid[32];
	UINT_8	ucSsidLen;
} P2P_SSID_STRUCT_T, *P_P2P_SSID_STRUCT_T;


typedef struct _P2P_SCAN_REQ_INFO_T {
	ENUM_SCAN_TYPE_T	eScanType;
	ENUM_SCAN_CHANNEL	eChannelSet;
	UINT_16			u2PassiveDewellTime;
	UINT_8			ucSeqNumOfScnMsg;
	BOOLEAN			fgIsAbort;
	BOOLEAN			fgIsScanRequest;
	UINT_8			ucNumChannelList;
	RF_CHANNEL_INFO_T	arScanChannelList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_32			u4BufLength;
	UINT_8			aucIEBuf[MAX_IE_LENGTH];
	UINT_8			ucSsidNum;
	P2P_SSID_STRUCT_T	arSsidStruct[SCN_SSID_MAX_NUM];	/* Currently we can only take one SSID scan request */
} P2P_SCAN_REQ_INFO_T, *P_P2P_SCAN_REQ_INFO_T;

typedef struct _P2P_CHNL_REQ_INFO_T {
	LINK_T			rP2pChnlReqLink;
	BOOLEAN			fgIsChannelRequested;
	UINT_8			ucSeqNumOfChReq;
	UINT_64			u8Cookie;
	UINT_8			ucReqChnlNum;
	ENUM_BAND_T		eBand;
	ENUM_CHNL_EXT_T		eChnlSco;
	UINT_8			ucOriChnlNum;
	ENUM_CHANNEL_WIDTH_T	eChannelWidth;	/*VHT operation ie */
	UINT_8			ucCenterFreqS1;
	UINT_8			ucCenterFreqS2;
	ENUM_BAND_T		eOriBand;
	ENUM_CHNL_EXT_T		eOriChnlSco;
	UINT_32			u4MaxInterval;
	ENUM_CH_REQ_TYPE_T	eChnlReqType;
#if CFG_SUPPORT_NFC_BEAM_PLUS
	UINT_32			NFC_BEAM;	/*NFC Beam + Indication */
#endif
} P2P_CHNL_REQ_INFO_T, *P_P2P_CHNL_REQ_INFO_T;

/* Glubal Connection Settings. */
struct _P2P_CONNECTION_SETTINGS_T {
	UINT_8	ucRfChannelListSize;
#if P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE
	UINT_8	aucChannelEntriesField[P2P_MAX_SUPPORTED_CHANNEL_LIST_SIZE];
#endif

	BOOLEAN fgIsApMode;
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
	BOOLEAN fgIsWPSMode;
#endif
};

typedef struct _NOA_TIMING_T {
	BOOLEAN fgIsInUse;	/* Indicate if this entry is in use or not */
	UINT_8	ucCount;	/* Count */

	UINT_8	aucReserved[2];

	UINT_32 u4Duration;	/* Duration */
	UINT_32 u4Interval;	/* Interval */
	UINT_32 u4StartTime;	/* Start Time */
} NOA_TIMING_T, *P_NOA_TIMING_T;


struct _P2P_SPECIFIC_BSS_INFO_T {
	/* For GO(AP) Mode - Compose TIM IE */
	UINT_16					u2SmallestAID;
	UINT_16					u2LargestAID;
	UINT_8					ucBitmapCtrl;
	/* UINT_8                  aucPartialVirtualBitmap[MAX_LEN_TIM_PARTIAL_BMP]; */

	/* For GC/GO OppPS */
	BOOLEAN					fgEnableOppPS;
	UINT_16					u2CTWindow;

	/* For GC/GO NOA */
	UINT_8					ucNoAIndex;
	UINT_8					ucNoATimingCount;	/* Number of NoA Timing */
	NOA_TIMING_T				arNoATiming[P2P_MAXIMUM_NOA_COUNT];

	BOOLEAN					fgIsNoaAttrExisted;

	/* For P2P Device */
	UINT_8					ucRegClass;		/* Regulatory Class for channel. */
	UINT_8					ucListenChannel;	/* Linten Channel only on channels 1, 6 and 11
									 *in the 2.4 GHz. */

	UINT_8					ucPreferredChannel;	/* Operating Channel, should be one of channel
									 *list in p2p connection settings. */
	ENUM_CHNL_EXT_T				eRfSco;
	ENUM_BAND_T				eRfBand;

	/* Extened Listen Timing. */
	UINT_16					u2AvailabilityPeriod;
	UINT_16					u2AvailabilityInterval;

	UINT_16					u2AttributeLen;
	UINT_8					aucAttributesCache[P2P_MAXIMUM_ATTRIBUTES_CACHE_SIZE];

	UINT_16					u2WscAttributeLen;
	UINT_8					aucWscAttributesCache[WPS_MAXIMUM_ATTRIBUTES_CACHE_SIZE];

	UINT_8					aucGroupID[MAC_ADDR_LEN];
	UINT_16					u2GroupSsidLen;
	UINT_8					aucGroupSsid[ELEM_MAX_LEN_SSID];

	PARAM_CUSTOM_NOA_PARAM_STRUC_T		rNoaParam;
	PARAM_CUSTOM_OPPPS_PARAM_STRUC_T	rOppPsParam;

	UINT_16                 u2WpaIeLen;
    	UINT_8                  aucWpaIeBuffer[ELEM_HDR_LEN + ELEM_MAX_LEN_WPA];
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

#endif	/*_P2P_H */
