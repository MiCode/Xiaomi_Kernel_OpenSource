/*******************************************************************************
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
 ******************************************************************************/
/*
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/include
       /gl_wext_priv.h#3
 */

/*! \file   gl_wext_priv.h
 *    \brief  This file includes private ioctl support.
 */


#ifndef _GL_WEXT_PRIV_H
#define _GL_WEXT_PRIV_H
/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
/* If it is set to 1, iwpriv will support register read/write */
#define CFG_SUPPORT_PRIV_MCR_RW         1

/* Stat CMD will have different format due to different algorithm support */
#if (defined(MT6632) || defined(MT7668))
#define CFG_SUPPORT_RA_GEN			0
#define CFG_SUPPORT_TXPOWER_INFO		0
#else
#define CFG_SUPPORT_RA_GEN			1
#define CFG_SUPPORT_TXPOWER_INFO		1
#endif

/*******************************************************************************
 *			E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
extern char *HW_TX_MODE_STR[];
extern char *HW_TX_RATE_CCK_STR[];
extern char *HW_TX_RATE_OFDM_STR[];
extern char *HW_TX_RATE_BW[];

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* New wireless extensions API - SET/GET convention (even ioctl numbers are
 * root only)
 */
#define IOCTL_SET_INT                   (SIOCIWFIRSTPRIV + 0)
#define IOCTL_GET_INT                   (SIOCIWFIRSTPRIV + 1)

#define IOCTL_SET_ADDRESS               (SIOCIWFIRSTPRIV + 2)
#define IOCTL_GET_ADDRESS               (SIOCIWFIRSTPRIV + 3)
#define IOCTL_SET_STR                   (SIOCIWFIRSTPRIV + 4)
#define IOCTL_GET_STR                   (SIOCIWFIRSTPRIV + 5)
#define IOCTL_SET_KEY                   (SIOCIWFIRSTPRIV + 6)
#define IOCTL_GET_KEY                   (SIOCIWFIRSTPRIV + 7)
#define IOCTL_SET_STRUCT                (SIOCIWFIRSTPRIV + 8)
#define IOCTL_GET_STRUCT                (SIOCIWFIRSTPRIV + 9)
#if CFG_MTK_ENGINEER_MODE_SUPPORT
#define IOCTL_SET_STRUCT_FOR_EM         (SIOCIWFIRSTPRIV + 11)
#endif
#define IOCTL_SET_INTS                  (SIOCIWFIRSTPRIV + 12)
#define IOCTL_GET_INTS                  (SIOCIWFIRSTPRIV + 13)
#define IOCTL_SET_DRIVER                (SIOCIWFIRSTPRIV + 14)
#define IOCTL_GET_DRIVER                (SIOCIWFIRSTPRIV + 15)

#if CFG_SUPPORT_QA_TOOL
#define IOCTL_QA_TOOL_DAEMON			(SIOCIWFIRSTPRIV + 16)
#define IOCTL_IWPRIV_ATE                (SIOCIWFIRSTPRIV + 17)
#endif

#define IOC_AP_GET_STA_LIST     (SIOCIWFIRSTPRIV+19)
#define IOC_AP_SET_MAC_FLTR     (SIOCIWFIRSTPRIV+21)
#define IOC_AP_SET_CFG          (SIOCIWFIRSTPRIV+23)
#define IOC_AP_STA_DISASSOC     (SIOCIWFIRSTPRIV+25)

#define PRIV_CMD_REG_DOMAIN             0
#define PRIV_CMD_BEACON_PERIOD          1
#define PRIV_CMD_ADHOC_MODE             2

#if CFG_TCP_IP_CHKSUM_OFFLOAD
#define PRIV_CMD_CSUM_OFFLOAD       3
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

#define PRIV_CMD_ROAMING                4
#define PRIV_CMD_VOIP_DELAY             5
#define PRIV_CMD_POWER_MODE             6

#define PRIV_CMD_WMM_PS                 7
#define PRIV_CMD_BT_COEXIST             8
#define PRIV_GPIO2_MODE                 9

#define PRIV_CUSTOM_SET_PTA			10
#define PRIV_CUSTOM_CONTINUOUS_POLL     11
#define PRIV_CUSTOM_SINGLE_ANTENNA		12
#define PRIV_CUSTOM_BWCS_CMD			13
#define PRIV_CUSTOM_DISABLE_BEACON_DETECTION	14	/* later */
#define PRIV_CMD_OID                    15
#define PRIV_SEC_MSG_OID                16

#define PRIV_CMD_TEST_MODE              17
#define PRIV_CMD_TEST_CMD               18
#define PRIV_CMD_ACCESS_MCR             19
#define PRIV_CMD_SW_CTRL                20

#if 1				/* ANTI_PRIVCY */
#define PRIV_SEC_CHECK_OID              21
#endif

#define PRIV_CMD_WSC_PROBE_REQ          22

#define PRIV_CMD_P2P_VERSION                   23

#define PRIV_CMD_GET_CH_LIST            24

#define PRIV_CMD_SET_TX_POWER           25

#define PRIV_CMD_BAND_CONFIG            26

#define PRIV_CMD_DUMP_MEM               27

#define PRIV_CMD_P2P_MODE               28

#if CFG_SUPPORT_QA_TOOL
#define PRIV_QACMD_SET					29
#endif

#define PRIV_CMD_MET_PROFILING          33

#if CFG_WOW_SUPPORT
#define PRIV_CMD_SET_WOW_ENABLE			34
#define PRIV_CMD_SET_WOW_PAR			35
#define MDNS_CMD_ENABLE					1
#define MDNS_CMD_DISABLE				2
#define MDNS_CMD_ADD_RECORD				3
#define MDNS_CMD_DEL_RECORD				4
#endif

#define PRIV_CMD_SET_SER                37

#if CFG_SUPPORT_WAC
#define PRIV_CMD_WAC_IE				38
#endif

/* 802.3 Objects (Ethernet) */
#define OID_802_3_CURRENT_ADDRESS           0x01010102

/* IEEE 802.11 OIDs */
#define OID_802_11_SUPPORTED_RATES              0x0D01020E
#define OID_802_11_CONFIGURATION                0x0D010211

/* PnP and PM OIDs, NDIS default OIDS */
#define OID_PNP_SET_POWER                               0xFD010101

#define OID_CUSTOM_OID_INTERFACE_VERSION                0xFFA0C000

/* MT5921 specific OIDs */
#define OID_CUSTOM_BT_COEXIST_CTRL                      0xFFA0C580
#define OID_CUSTOM_POWER_MANAGEMENT_PROFILE             0xFFA0C581
#define OID_CUSTOM_PATTERN_CONFIG                       0xFFA0C582
#define OID_CUSTOM_BG_SSID_SEARCH_CONFIG                0xFFA0C583
#define OID_CUSTOM_VOIP_SETUP                           0xFFA0C584
#define OID_CUSTOM_ADD_TS                               0xFFA0C585
#define OID_CUSTOM_DEL_TS                               0xFFA0C586
#define OID_CUSTOM_SLT                               0xFFA0C587
#define OID_CUSTOM_ROAMING_EN                           0xFFA0C588
#define OID_CUSTOM_WMM_PS_TEST                          0xFFA0C589
#define OID_CUSTOM_COUNTRY_STRING                       0xFFA0C58A
#define OID_CUSTOM_MULTI_DOMAIN_CAPABILITY              0xFFA0C58B
#define OID_CUSTOM_GPIO2_MODE                           0xFFA0C58C
#define OID_CUSTOM_CONTINUOUS_POLL                      0xFFA0C58D
#define OID_CUSTOM_DISABLE_BEACON_DETECTION             0xFFA0C58E

/* CR1460, WPS privacy bit check disable */
#define OID_CUSTOM_DISABLE_PRIVACY_CHECK                0xFFA0C600

/* Precedent OIDs */
#define OID_CUSTOM_MCR_RW                               0xFFA0C801
#define OID_CUSTOM_EEPROM_RW                            0xFFA0C803
#define OID_CUSTOM_SW_CTRL                              0xFFA0C805
#define OID_CUSTOM_MEM_DUMP                             0xFFA0C807

/* RF Test specific OIDs */
#define OID_CUSTOM_TEST_MODE                            0xFFA0C901
#define OID_CUSTOM_TEST_RX_STATUS                       0xFFA0C903
#define OID_CUSTOM_TEST_TX_STATUS                       0xFFA0C905
#define OID_CUSTOM_ABORT_TEST_MODE                      0xFFA0C906
#define OID_CUSTOM_MTK_WIFI_TEST                        0xFFA0C911
#define OID_CUSTOM_TEST_ICAP_MODE                       0xFFA0C913

/* BWCS */
#define OID_CUSTOM_BWCS_CMD                             0xFFA0C931
#define OID_CUSTOM_SINGLE_ANTENNA                       0xFFA0C932
#define OID_CUSTOM_SET_PTA                              0xFFA0C933

/* NVRAM */
#define OID_CUSTOM_MTK_NVRAM_RW                         0xFFA0C941
#define OID_CUSTOM_CFG_SRC_TYPE                         0xFFA0C942
#define OID_CUSTOM_EEPROM_TYPE                          0xFFA0C943

#if CFG_SUPPORT_WAPI
#define OID_802_11_WAPI_MODE                            0xFFA0CA00
#define OID_802_11_WAPI_ASSOC_INFO                      0xFFA0CA01
#define OID_802_11_SET_WAPI_KEY                         0xFFA0CA02
#endif

#if CFG_SUPPORT_WPS2
#define OID_802_11_WSC_ASSOC_INFO                       0xFFA0CB00
#endif

#if CFG_SUPPORT_LOWLATENCY_MODE
#define OID_CUSTOM_LOWLATENCY_MODE			0xFFA0CC00
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

#define OID_IPC_WIFI_LOG_UI                             0xFFA0CC01
#define OID_IPC_WIFI_LOG_LEVEL                          0xFFA0CC02

#if CFG_SUPPORT_NCHO
#define CMD_NCHO_COMP_TIMEOUT			1500	/* ms */
#define CMD_NCHO_AF_DATA_LENGTH			1040
#endif

/* Define magic key of test mode (Don't change it for future compatibity) */
#define PRIV_CMD_TEST_MAGIC_KEY                         2011
#define PRIV_CMD_TEST_MAGIC_KEY_ICAP                         2013

/* Customized MCC stay time ratio and the stay time in units of us */
#if CFG_SUPPORT_ADJUST_MCC_MODE_SET
#define MCC_TIME_3		29000
#define MCC_TIME_5		48200
#define MCC_TIME_7		67400
#endif /*CFG_SUPPORT_ADJUST_MCC_MODE_SET*/

#if CFG_SUPPORT_ADVANCE_CONTROL
#define CMD_SW_DBGCTL_ADVCTL_SET_ID 0xa1260000
#define CMD_SW_DBGCTL_ADVCTL_GET_ID 0xb1260000

#define CMD_SET_NOISE		"SET_NOISE"
#define CMD_GET_NOISE           "GET_NOISE"
#define CMD_TRAFFIC_REPORT      "TRAFFIC_REPORT"
#define CMD_SET_POP		"SET_POP"
#define CMD_GET_POP             "GET_POP"
#define CMD_SET_ED		"SET_ED"
#define CMD_GET_ED              "GET_ED"
#define CMD_SET_PD		"SET_PD"
#define CMD_GET_PD              "GET_PD"
#define CMD_SET_MAX_RFGAIN	"SET_MAX_RFGAIN"
#define CMD_GET_MAX_RFGAIN      "GET_MAX_RFGAIN"
#define CMD_NOISE_HISTOGRAM     "NOISE_HISTOGRAM"
#if CFG_RX_SINGLE_CHAIN_SUPPORT
#define CMD_SET_RXC             "SET_RXC"
#define CMD_GET_RXC             "GET_RXC"
#endif
#define CMD_COEX_CONTROL        "COEX_CONTROL"
#define CMD_SET_ADM_CTRL        "SET_ADM"
#define CMD_GET_ADM_CTRL        "GET_ADM"
#define CMD_SET_BCN_TH          "SET_BCN_TH"
#define CMD_GET_BCN_TH          "GET_BCN_TH"
#define CMD_GET_BCNTIMEOUT_NUM  "GET_BCNTIMEOUT_NUM"
#if CFG_ENABLE_1RPD_MMPS_CTRL
#define CMD_SET_1RPD			"SET_1RPD"
#define CMD_GET_1RPD			"GET_1RPD"
#define CMD_SET_MMPS			"SET_MMPS"
#define CMD_GET_MMPS			"GET_MMPS"
#endif /* CFG_ENABLE_1RPD_MMPS_CTRL */
#if CFG_ENABLE_DEWEIGHTING_CTRL
#define CMD_SET_DEWEIGHTING_TH		"SET_DEWEIGHTING_TH"
#define CMD_GET_DEWEIGHTING_TH		"GET_DEWEIGHTING_TH"
#define CMD_GET_DEWEIGHTING_NOISE	"GET_DEWEIGHTING_NOISE"
#define CMD_GET_DEWEIGHTING_WEIGHT	"GET_DEWEIGHTING_WEIGHT"
#endif /* CFG_ENABLE_DEWEIGHTING_CTRL */

enum {
	CMD_ADVCTL_NOISE_ID = 1,
	CMD_ADVCTL_POP_ID,
	CMD_ADVCTL_ED_ID,
	CMD_ADVCTL_PD_ID,
	CMD_ADVCTL_MAX_RFGAIN_ID,
	CMD_ADVCTL_ADM_CTRL_ID,
	CMD_ADVCTL_BCN_TH_ID = 9,
	CMD_ADVCTL_DEWEIGHTING_TH_ID,
	CMD_ADVCTL_DEWEIGHTING_NOISE_ID,
	CMD_ADVCTL_DEWEIGHTING_WEIGHT_ID,
	CMD_ADVCTL_ACT_INTV_ID,
	CMD_ADVCTL_1RPD,
	CMD_ADVCTL_MMPS,
#if CFG_RX_SINGLE_CHAIN_SUPPORT
	CMD_ADVCTL_RXC_ID = 17,
#endif
	CMD_ADVCTL_SNR_ID = 18,
	CMD_ADVCTL_BCNTIMOUT_NUM_ID = 19,
	CMD_ADVCTL_EVERY_TBTT_ID = 20,
	CMD_ADVCTL_MAX
};
#endif /* CFG_SUPPORT_ADVANCE_CONTROL */

#if (CFG_SUPPORT_TXPOWER_INFO == 1)
#define TX_POWER_SHOW_INFO                              0x7
#endif

#define AGG_RANGE_SEL_NUM                               7
#define AGG_RANGE_SEL_0_MASK                            BITS(0, 7)
#define AGG_RANGE_SEL_0_OFFSET                          0
#define AGG_RANGE_SEL_1_MASK                            BITS(8, 15)
#define AGG_RANGE_SEL_1_OFFSET                          8
#define AGG_RANGE_SEL_2_MASK                            BITS(16, 23)
#define AGG_RANGE_SEL_2_OFFSET                          16
#define AGG_RANGE_SEL_3_MASK                            BITS(24, 31)
#define AGG_RANGE_SEL_3_OFFSET                          24
#define AGG_RANGE_SEL_4_MASK                            AGG_RANGE_SEL_0_MASK
#define AGG_RANGE_SEL_4_OFFSET                          AGG_RANGE_SEL_0_OFFSET
#define AGG_RANGE_SEL_5_MASK                            AGG_RANGE_SEL_1_MASK
#define AGG_RANGE_SEL_5_OFFSET                          AGG_RANGE_SEL_1_OFFSET
#define AGG_RANGE_SEL_6_MASK                            AGG_RANGE_SEL_2_MASK
#define AGG_RANGE_SEL_6_OFFSET                          AGG_RANGE_SEL_2_OFFSET

enum {
	COEX_REF_TABLE_ID_ISO_DETECTION_VALUE = 0,
	COEX_REF_TABLE_ID_COEX_BT_PROFILE = 1,
	COEX_REF_TABLE_ID_RX_BASIZE = 2,
	COEX_REF_TABLE_ID_ARB_MODE = 3,
	COEX_REF_TABLE_ID_PROT_FRM_TYPE = 4,
	COEX_REF_TABLE_ID_CFG_COEXISOCTRL = 5,
	COEX_REF_TABLE_ID_CFG_COEXMODECTRL = 6,
	COEX_REF_TABLE_ID_CFG_FDDPERPKT = 7,
	COEX_REF_TABLE_ID_CFG_BT_FIX_POWER = 8,
	COEX_REF_TABLE_ID_CFG_BT_FDD_GAIN = 9,
	COEX_REF_TABLE_ID_CFG_BT_FDD_POWER = 10,
	COEX_REF_TABLE_ID_CFG_WF_FDD_POWER = 11,
	COEX_REF_TABLE_ID_NUM
};

/*  BT Profile macros */
#define VAR_BT_PROF_MASK_ALL 0x0000FFFF
/* these defined name are the same as name in FW  */
#define VAR_BT_PROF_NONE           0x0000
#define VAR_BT_PROF_SCO            0x0001
#define VAR_BT_PROF_A2DP           0x0002
#define VAR_BT_PROF_LINK_CONNECTED 0x0004
#define VAR_BT_PROF_HID            0x0008
#define VAR_BT_PROF_PAGE           0x0010
#define VAR_BT_PROF_INQUIRY        0x0020
#define VAR_BT_PROF_ESCO           0x0040
#define VAR_BT_PROF_MULTI_HID      0x0080
#define VAR_BT_PROF_BLE_VOBLE      0x0200
#define VAR_BT_PROF_BT_A2DP_SINK   0x0400

#define COEX_BCM_IS_BT_NONE(x) \
(((x) & VAR_BT_PROF_MASK_ALL) == VAR_BT_PROF_NONE)
#define COEX_BCM_IS_BT_SCO(x) ((x) & VAR_BT_PROF_SCO)
#define COEX_BCM_IS_BT_A2DP(x) ((x) & VAR_BT_PROF_A2DP)
#define COEX_BCM_IS_BT_LINK_CONNECTED(x) ((x) & VAR_BT_PROF_LINK_CONNECTED)
#define COEX_BCM_IS_BT_HID(x) ((x) & VAR_BT_PROF_HID)
#define COEX_BCM_IS_BT_PAGE(x) ((x) & VAR_BT_PROF_PAGE)
#define COEX_BCM_IS_BT_INQUIRY(x) ((x) & VAR_BT_PROF_INQUIRY)
#define COEX_BCM_IS_BT_ESCO(x) ((x) & VAR_BT_PROF_ESCO)
#define COEX_BCM_IS_BT_MULTI_HID(x) ((x) & VAR_BT_PROF_MULTI_HID)
#define COEX_BCM_IS_BT_BLE_VOBLE(x) ((x) & VAR_BT_PROF_BLE_VOBLE)
#define COEX_BCM_IS_BT_A2DP_SINK(x) ((x) & VAR_BT_PROF_BT_A2DP_SINK)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
/* NIC BBCR configuration entry structure */
struct PRIV_CONFIG_ENTRY {
	uint8_t ucOffset;
	uint8_t ucValue;
};

typedef uint32_t(*PFN_OID_HANDLER_FUNC_REQ) (
	IN void *prAdapter,
	IN OUT void *pvBuf, IN uint32_t u4BufLen,
	OUT uint32_t *pu4OutInfoLen);

enum ENUM_OID_METHOD {
	ENUM_OID_GLUE_ONLY,
	ENUM_OID_GLUE_EXTENSION,
	ENUM_OID_DRIVER_CORE
};

/* OID set/query processing entry */
struct WLAN_REQ_ENTRY {
	uint32_t rOid;		/* OID */
	uint8_t *pucOidName;	/* OID name text */
	u_int8_t fgQryBufLenChecking;
	u_int8_t fgSetBufLenChecking;
	enum ENUM_OID_METHOD eOidMethod;
	uint32_t u4InfoBufLen;
	PFN_OID_HANDLER_FUNC_REQ pfOidQueryHandler; /* PFN_OID_HANDLER_FUNC */
	PFN_OID_HANDLER_FUNC_REQ pfOidSetHandler; /* PFN_OID_HANDLER_FUNC */
};

struct NDIS_TRANSPORT_STRUCT {
	uint32_t ndisOidCmd;
	uint32_t inNdisOidlength;
	uint32_t outNdisOidLength;
	uint8_t ndisOidContent[16];
};

enum AGG_RANGE_TYPE_T {
	ENUM_AGG_RANGE_TYPE_TX = 0,
	ENUM_AGG_RANGE_TYPE_TRX = 1,
	ENUM_AGG_RANGE_TYPE_RX = 2
};

#if CFG_SUPPORT_ADJUST_MCC_MODE_SET
enum ENUM_MCC_MODE {
	MCC_FAIR,
	MCC_FAV_STA,
	MCC_FAV_P2P,
	MCC_MODE_NUM
};
#endif

/*******************************************************************************
 *			P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *			P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *			M A C R O S
 *******************************************************************************
 */
#define HW_TX_RATE_TO_MODE(_x)			(((_x) & (0x7 << 6)) >> 6)
#define HW_TX_RATE_TO_MCS(_x, _mode)		((_x) & (0x3f))
#define HW_TX_RATE_TO_NSS(_x)			(((_x) & (0x3 << 9)) >> 9)
#define HW_TX_RATE_TO_STBC(_x)			(((_x) & (0x1 << 11)) >> 11)

#define TX_VECTOR_GET_TX_RATE(_txv)     (((_txv)->u4TxVector1) & \
					BITS(0, 6))
#define TX_VECTOR_GET_TX_LDPC(_txv)     ((((_txv)->u4TxVector1) >> 7) & \
					BIT(0))
#define TX_VECTOR_GET_TX_STBC(_txv)     ((((_txv)->u4TxVector1) >> 8) & \
					BITS(0, 1))
#define TX_VECTOR_GET_TX_FRMODE(_txv)   ((((_txv)->u4TxVector1) >> 10) & \
					BITS(0, 1))
#define TX_VECTOR_GET_TX_MODE(_txv)     ((((_txv)->u4TxVector1) >> 12) & \
					BITS(0, 2))
#define TX_VECTOR_GET_TX_NSTS(_txv)     ((((_txv)->u4TxVector1) >> 21) & \
					BITS(0, 1))
#define TX_VECTOR_GET_TX_PWR(_txv)      ( \
					 ((_txv)->u4TxVector1 & BIT(30)) \
					 ? ((((_txv)->u4TxVector1 >> 24) & \
					   BITS(0, 6)) - 0x80) \
					 : (((_txv)->u4TxVector1 >> 24) & \
					   BITS(0, 6)) \
					)
#define TX_VECTOR_GET_BF_EN(_txv)       ((((_txv)->u4TxVector2) >> 31) & \
					BIT(0))
#define TX_VECTOR_GET_DYN_BW(_txv)      ((((_txv)->u4TxVector4) >> 31) & \
					BIT(0))
#define TX_VECTOR_GET_NO_SOUNDING(_txv) ((((_txv)->u4TxVector4) >> 28) & \
					BIT(0))
#define TX_VECTOR_GET_TX_SGI(_txv)      ((((_txv)->u4TxVector4) >> 27) & \
					BIT(0))

#define TX_RATE_MODE_CCK	0
#define TX_RATE_MODE_OFDM	1
#define TX_RATE_MODE_HTMIX	2
#define TX_RATE_MODE_HTGF	3
#define TX_RATE_MODE_VHT	4
#define MAX_TX_MODE		5

/*******************************************************************************
 *			F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

int
priv_set_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN char *pcExtra);

int
priv_get_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN OUT char *pcExtra);

int
priv_set_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo,
	      IN union iwreq_data *prIwReqData, IN char *pcExtra);

int
priv_get_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo,
	      IN union iwreq_data *prIwReqData, IN OUT char *pcExtra);

int
priv_set_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo,
		IN union iwreq_data *prIwReqData, IN char *pcExtra);

int
priv_get_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo,
		IN union iwreq_data *prIwReqData, IN OUT char *pcExtra);

#if CFG_SUPPORT_NCHO
uint8_t CmdString2HexParse(IN uint8_t *InStr,
			   OUT uint8_t **OutStr, OUT uint8_t *OutLen);
#endif

int
priv_set_driver(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo,
		IN union iwreq_data *prIwReqData, IN OUT char *pcExtra);

int
priv_set_ap(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo,
		IN union iwreq_data *prIwReqData, IN OUT char *pcExtra);

int priv_support_ioctl(IN struct net_device *prDev,
		       IN OUT struct ifreq *prReq, IN int i4Cmd);

int priv_support_driver_cmd(IN struct net_device *prDev,
			    IN OUT struct ifreq *prReq, IN int i4Cmd);

#ifdef CFG_ANDROID_AOSP_PRIV_CMD
int android_private_support_driver_cmd(IN struct net_device *prDev,
IN OUT struct ifreq *prReq, IN int i4Cmd);
#endif /* CFG_ANDROID_AOSP_PRIV_CMD */

int32_t priv_driver_cmds(IN struct net_device *prNetDev,
			 IN int8_t *pcCommand, IN int32_t i4TotalLen);

int priv_driver_set_cfg(IN struct net_device *prNetDev,
			IN char *pcCommand, IN int i4TotalLen);

#if CFG_SUPPORT_QA_TOOL
int
priv_ate_set(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN char *pcExtra);
#endif

char *hw_rate_ofdm_str(uint16_t ofdm_idx);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _GL_WEXT_PRIV_H */
