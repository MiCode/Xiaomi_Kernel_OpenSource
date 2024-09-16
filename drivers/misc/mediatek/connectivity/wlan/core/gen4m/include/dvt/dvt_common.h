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
 ** Id: include/dvt_common.h
 */

/*! \file   "dvt_common.h"
 *    \brief This file contains the declairations of sys dvt command
 */

#ifndef _DVT_COMMON_H
#define _DVT_COMMON_H

#if CFG_SUPPORT_WIFI_SYSDVT

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *			E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define PID_SIZE 256
#define TXS_LIST_ELEM_NUM 4096
#define TX_TEST_UNLIMITIED 0xFFFF
#define TX_TEST_UP_UNDEF   0xFF
/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define IS_SKIP_CH_CHECK(_prAdapter) \
	((_prAdapter)->auto_dvt && (_prAdapter)->auto_dvt->skip_legal_ch_enable)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
enum ENUM_SYSDVT_CTRL_EXT_T {
	EXAMPLE_FEATURE_ID = 0,
	SYSDVT_CTRL_EXT_NUM
};

enum ENUM_AUTOMATION_TXS_TESTYPE {
	TXS_INIT = 0,
	TXS_COUNT_TEST,
	TXS_BAR_TEST,
	TXS_DEAUTH_TEST,
	TXS_RTS_TEST,
	TXS_BA_TEST,
	TXS_DUMP_DATA,
	TXS_NUM
};

#if CFG_TCP_IP_CHKSUM_OFFLOAD
enum ENUM_AUTOMATION_CSO_TESTYPE {
	CSO_TX_IPV4 = BIT(0),
	CSO_TX_IPV6 = BIT(1),
	CSO_TX_TCP = BIT(2),
	CSO_TX_UDP = BIT(3),
	CSO_RX_IPV4 = BIT(4),
	CSO_RX_IPV6 = BIT(5),
	CSO_RX_TCP = BIT(6),
	CSO_RX_UDP = BIT(7),
};
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

enum ENUM_AUTOMATION_INIT_TYPE {
	TXS = 0,
	RXV,
#if (CFG_SUPPORT_DMASHDL_SYSDVT)
	DMASHDL,
#endif
	CSO,
	SKIP_CH
};

enum ENUM_AUTOMATION_FRAME_TYPE {
	AUTOMATION_MANAGEMENT = 10,
	AUTOMATION_CONTROL,
	AUTOMATION_DATA
};

struct SYSDVT_CTRL_EXT_T {
	/* DWORD_0 - Common Part */
	uint8_t ucCmdVer;
	uint8_t aucPadding0[1];
	uint16_t u2CmdLen;	/* Cmd size including common part and body */

	/* DWORD_N - Body Part */
	uint32_t u4FeatureIdx;	/* Feature  ID */
	uint32_t u4Type;	/* Test case  ID (Type) */
	uint32_t u4Lth;	/* dvt parameter's data struct size (Length) */
	uint8_t u1cBuffer[0];	/* dvt parameter's data struct (Value) */
};

struct SYS_DVT_HANDLER {
	uint32_t u4FeatureIdx;
	uint8_t *str_feature_dvt;
	int32_t (*pFeatureDvtHandler)(struct ADAPTER *, uint8_t *);
	void (*pDoneHandler)(struct ADAPTER *, struct CMD_INFO *, uint8_t *);
};

struct TXS_CHECK_ITEM {
	uint32_t time_stamp;
};

struct TXS_LIST_ENTRY {
	struct list_head mList;
	uint8_t wlan_idx;
};

struct TXS_LIST_POOL {
	struct TXS_LIST_ENTRY Entry[TXS_LIST_ELEM_NUM];
	struct list_head List;
};

struct TXS_FREE_LIST_POOL {
	struct TXS_LIST_ENTRY head;
	struct TXS_LIST_POOL pool_head;
	uint32_t entry_number;
	spinlock_t Lock;
	uint32_t txs_list_cnt;
};

struct TXS_LIST {
	uint32_t Num;
	spinlock_t lock;
	struct TXS_LIST_ENTRY pHead[PID_SIZE];
	struct TXS_FREE_LIST_POOL *pFreeEntrylist;
};

struct TXS_TEST {
	bool isinit;
	uint32_t test_type;
	uint8_t format;

	uint8_t	pid;
	uint8_t	received_pid;
	bool	stop_send_test;
	bool	duplicate_txs;

	/* statistic */
	uint32_t total_req;
	uint32_t total_rsp;
	struct TXS_LIST txs_list;
	struct TXS_CHECK_ITEM check_item[PID_SIZE];
};

struct RXV_TEST {
	bool enable;
	bool rxv_test_result;
	uint32_t rx_count;

	uint32_t rx_mode:3;
	uint32_t rx_rate:7;
	uint32_t rx_bw:2;
	uint32_t rx_sgi:1;
	uint32_t rx_stbc:2;
	uint32_t rx_ldpc:1;
	uint32_t rx_nss:1;
};

#if (CFG_SUPPORT_DMASHDL_SYSDVT)
struct DMASHDL_TEST {
	uint8_t dvt_item;
	uint8_t dvt_sub_item;
	uint8_t dvt_queue_idx;
	uint8_t dvt_ping_nums[32];
	uint8_t dvt_ping_seq[20];
};
#endif

struct AUTOMATION_DVT {
	uint8_t skip_legal_ch_enable;
	struct TXS_TEST txs;
	struct RXV_TEST rxv;
#if (CFG_SUPPORT_DMASHDL_SYSDVT)
	struct DMASHDL_TEST dmashdl;
#endif
	uint8_t cso_ctrl;
};

struct _FRAME_RTS {
	/* MAC header */
	uint16_t u2FrameCtrl;	/* Frame Control */
	uint16_t u2Duration;	/* Duration */
	uint8_t aucDestAddr[MAC_ADDR_LEN];	/* DA */
	uint8_t aucSrcAddr[MAC_ADDR_LEN];	/* SA */
};

/* 2-byte BAR CONTROL field in BAR frame */
struct _BAR_CONTROL {
	uint16_t ACKPolicy:1; /* 0:normal ack,  1:no ack. */
/*if this bit1, use  FRAME_MTBA_REQ,  if 0, use FRAME_BA_REQ */
	uint16_t MTID:1;
	uint16_t Compressed:1;
	uint16_t Rsv1:9;
	uint16_t TID:4;
};

/* 2-byte BA Starting Seq CONTROL field */
union _BASEQ_CONTROL {
	struct {
	uint16_t FragNum:4;	/* always set to 0 */
/* sequence number of the 1st MSDU for which this BAR is sent */
	uint16_t StartSeq:12;
	} field;
	uint16_t word;
};

struct _FRAME_BA {
	/* MAC header */
	uint16_t u2FrameCtrl;	/* Frame Control */
	uint16_t u2Duration;	/* Duration */
	uint8_t aucDestAddr[MAC_ADDR_LEN];	/* DA */
	uint8_t aucSrcAddr[MAC_ADDR_LEN];	/* SA */
	struct _BAR_CONTROL	BarControl;
	union _BASEQ_CONTROL	StartingSeq;
	unsigned char bitmask[8];
};
#endif	/* CFG_SUPPORT_WIFI_SYSDVT */

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

#if CFG_TCP_IP_CHKSUM_OFFLOAD
#define CSO_TX_IPV4_ENABLED(pAd)            \
	(pAd->auto_dvt && (pAd->auto_dvt->cso_ctrl & CSO_TX_IPV4))
#define CSO_TX_IPV6_ENABLED(pAd)            \
	(pAd->auto_dvt && (pAd->auto_dvt->cso_ctrl & CSO_TX_IPV6))
#define CSO_TX_UDP_ENABLED(pAd)            \
	(pAd->auto_dvt && (pAd->auto_dvt->cso_ctrl & CSO_TX_UDP))
#define CSO_TX_TCP_ENABLED(pAd)            \
	(pAd->auto_dvt && (pAd->auto_dvt->cso_ctrl & CSO_TX_TCP))
#endif

#define RXV_AUTODVT_DNABLED(pAd)			\
	(pAd->auto_dvt && pAd->auto_dvt->rxv.enable)

/*******************************************************************************
 *			F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
#if CFG_SUPPORT_WIFI_SYSDVT
struct TXS_LIST_ENTRY *GetTxsEntryFromFreeList(void);

int SendRTS(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	PFN_TX_DONE_HANDLER pfTxDoneHandler);

int SendBA(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	PFN_TX_DONE_HANDLER pfTxDoneHandler);

bool send_add_txs_queue(uint8_t pid, uint8_t wlan_idx);

bool receive_del_txs_queue(
	uint32_t sn, uint8_t pid,
	uint8_t wlan_idx,
	uint32_t time_stamp);

int priv_driver_txs_test(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen);

int priv_driver_txs_test_result(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen);

int is_frame_test(struct ADAPTER *pAd, uint8_t send_received);

uint32_t AutomationTxDone(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

int priv_driver_set_tx_test(
	IN struct net_device *prNetDev, IN char *pcCommand,
	IN int i4TotalLen);
int priv_driver_set_tx_test_ac(
	IN struct net_device *prNetDev, IN char *pcCommand,
	IN int i4TotalLen);

/* RXV Test */
int priv_driver_rxv_test(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen);

int priv_driver_rxv_test_result(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen);

void connac2x_rxv_correct_test(
	IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb);

/* CSO Test */
int priv_driver_cso_test(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen);

/* skip legal channel sanity check for fpga dvt */
int priv_driver_skip_legal_ch_check(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen);

bool AutomationInit(struct ADAPTER *pAd, int32_t auto_type);

#endif	/* CFG_SUPPORT_WIFI_SYSDVT */
#endif	/* _DVT_COMMON_H */

