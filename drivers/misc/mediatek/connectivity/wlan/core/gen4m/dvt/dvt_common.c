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
 ** Id: include/dvt_common.c
 */

/*! \file dvt_common.c
 *    \brief This file contains the declairations of sys dvt command
 */

/*******************************************************************************
 *                    C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

/*******************************************************************************
 *                    F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                    P R I V A T E	 D A T A
 *******************************************************************************
 */
struct AUTOMATION_DVT automation_dvt;
struct TXS_FREE_LIST_POOL TxsFreeEntrylist;

/*******************************************************************************
 *                    F U N C T I O N S
 *******************************************************************************
 */

#if CFG_SUPPORT_WIFI_SYSDVT
/*
* This routine is used to init TXS pool for DVT
*/
void TxsPoolInit(void)
{
	if (TxsFreeEntrylist.txs_list_cnt == 0) {
		spin_lock_init(&TxsFreeEntrylist.Lock);
		INIT_LIST_HEAD(&TxsFreeEntrylist.pool_head.List);
		INIT_LIST_HEAD(&TxsFreeEntrylist.head.mList);
	}

	TxsFreeEntrylist.txs_list_cnt++;
}

/*
* This routine is used to uninit TXS pool for DVT
*/
void TxsPoolUnInit(void)
{
	struct list_head *prCur, *prNext;

	TxsFreeEntrylist.txs_list_cnt--;

	if (TxsFreeEntrylist.txs_list_cnt == 0) {
		struct TXS_LIST_POOL *pEntry = NULL;

		list_for_each_safe(prCur, prNext,
		&TxsFreeEntrylist.pool_head.List) {
			pEntry =
				list_entry(prCur,  struct TXS_LIST_POOL, List);
			list_del_init(&pEntry->List);
			list_del(prCur);
			if (pEntry == NULL)
				DBGLOG(REQ, LOUD, "pEntry null\n");
			kfree(pEntry);
		}
	}
}

/*
* This routine is used to test TXS function.
* init TXS DVT structure and start to test
*/
bool TxsInit(void)
{
	uint32_t i;
	struct TXS_LIST *list = &automation_dvt.txs.txs_list;

	if (automation_dvt.txs.isinit)
		return TRUE;

	automation_dvt.txs.isinit = FALSE;
	automation_dvt.txs.total_req = 0;
	automation_dvt.txs.total_rsp = 0;
	automation_dvt.txs.stop_send_test = TRUE;
	automation_dvt.txs.test_type = 0;
	automation_dvt.txs.pid = 1;

	spin_lock_init(&list->lock);

	for (i = 0; i < PID_SIZE; i++) {
		INIT_LIST_HEAD(&list->pHead[i].mList);
		automation_dvt.txs.check_item[i].time_stamp = 0;
	}

	list->Num = 0;
	TxsPoolInit();

	if (list_empty(&TxsFreeEntrylist.pool_head.List)) {
		struct TXS_LIST_POOL *Pool = NULL;
		struct TXS_LIST_POOL *pFreepool = NULL;
		struct TXS_LIST_ENTRY *pEntry = NULL;
		struct TXS_LIST_ENTRY *newEntry = NULL;

		Pool = kmalloc(sizeof(struct TXS_LIST_POOL), GFP_ATOMIC);
		pFreepool = &TxsFreeEntrylist.pool_head;
		list_add(&Pool->List, &pFreepool->List);
		pEntry = &TxsFreeEntrylist.head;

		for (i = 0; i < TXS_LIST_ELEM_NUM; i++) {
			newEntry = &Pool->Entry[i];
			list_add(&newEntry->mList, &pEntry->mList);
		}
	}

	list->pFreeEntrylist = &TxsFreeEntrylist;
	automation_dvt.txs.isinit = TRUE;
	return TRUE;
}

/*
* This routine is used to test TXS function.
* destroy TXS DVT structure
*/
bool TxsExit(void)
{
	uint32_t i = 0;
	unsigned long IrqFlags = 0;
	uint16_t wait_cnt = 0;
	struct TXS_LIST *list = &automation_dvt.txs.txs_list;

	automation_dvt.txs.isinit = FALSE;
	automation_dvt.txs.total_req = 0;
	automation_dvt.txs.total_rsp = 0;
	automation_dvt.txs.stop_send_test = TRUE;
	automation_dvt.txs.test_type = 0;
	automation_dvt.txs.pid = 1;

	while (automation_dvt.txs.txs_list.Num > 0) {
		DBGLOG(REQ, LOUD, "wait entry to be deleted\n");
		kalMsleep(100);/* OS_WAIT(10); */
		wait_cnt++;

		if (wait_cnt > 100)
			break;
	}

	spin_lock_irqsave(&list->lock, IrqFlags);

	for (i = 0; i < PID_SIZE; i++) {
		INIT_LIST_HEAD(&list->pHead[i].mList);
		automation_dvt.txs.check_item[i].time_stamp = 0;
	}

	spin_unlock_irqrestore(&list->lock, IrqFlags);
	list->Num = 0;
	TxsPoolUnInit();
	DBGLOG(REQ, LOUD, "TxsPoolUnInit done\n");

	return TRUE;
}

/*
* This routine is used to initial DVT of automation.
*/
bool AutomationInit(struct ADAPTER *pAd, int32_t auto_type)
{
	bool ret;

	ret = TRUE;
	if (!pAd)
		return FALSE;

	DBGLOG(REQ, LOUD, "In AutomationInit\n");

	if (pAd->auto_dvt == NULL) {
		kalMemZero(&automation_dvt, sizeof(struct AUTOMATION_DVT));
		pAd->auto_dvt = &automation_dvt;
		DBGLOG(REQ, LOUD, "AutomationInit\n");
	}

	switch (auto_type) {
	case TXS:
		ret = TxsInit();
		break;
	case RXV:
		break;
#if (CFG_SUPPORT_DMASHDL_SYSDVT)
	case DMASHDL:
		break;
#endif
	case CSO:
		break;
	case SKIP_CH:
		break;
	}

	return ret;
}

struct TXS_LIST_ENTRY *GetTxsEntryFromFreeList(void)
{
	struct TXS_LIST_ENTRY *pEntry = NULL;
	struct TXS_LIST_ENTRY *pheadEntry = NULL;
	struct TXS_FREE_LIST_POOL *pFreeEntrylist = NULL;
	unsigned long IrqFlags = 0;
	uint32_t i;

	pFreeEntrylist =
		automation_dvt.txs.txs_list.pFreeEntrylist;
	if (pFreeEntrylist == NULL)
		return NULL;

	spin_lock_irqsave(&pFreeEntrylist->Lock, IrqFlags);

	if (list_empty(&pFreeEntrylist->head.mList)) {
		struct TXS_LIST_POOL *Pool = NULL;
		struct TXS_LIST_POOL *pFreepool = NULL;

		DBGLOG(REQ, LOUD, "allocated new pool\n");
		Pool = kmalloc(sizeof(struct TXS_LIST_POOL), GFP_ATOMIC);
		pFreepool = &pFreeEntrylist->pool_head;
		list_add(&Pool->List, &pFreepool->List);
		pheadEntry = &pFreeEntrylist->head;

		for (i = 0; i < TXS_LIST_ELEM_NUM; i++) {
			pEntry = &Pool->Entry[i];
			list_add(&pEntry->mList, &pheadEntry->mList);
		}
		pFreeEntrylist->entry_number += TXS_LIST_ELEM_NUM;
	}

	pheadEntry = &pFreeEntrylist->head;
	if (!list_empty(&pheadEntry->mList)) {
		pEntry = list_entry(&pheadEntry->mList,
			struct TXS_LIST_ENTRY, mList);
		list_del(&pEntry->mList);
	}

	if (pEntry != NULL)
		pFreeEntrylist->entry_number -= 1;

	spin_unlock_irqrestore(&pFreeEntrylist->Lock, IrqFlags);
	return pEntry;
}

/*
* This routine is used to test TXS function.
* Send RTS
*/
int SendRTS(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	struct MSDU_INFO *prMsduInfo;
	struct _FRAME_RTS *prTxFrame;
	struct BSS_INFO *prBssInfo;
	uint16_t u2EstimatedFrameLen;
	unsigned long duration = 0;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	ASSERT(prBssInfo);

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD
		+ sizeof(struct _FRAME_RTS);

	/* Alloc MSDU_INFO */
	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
		u2EstimatedFrameLen);

	if (!prMsduInfo) {
		DBGLOG(REQ, WARN,
			"No MSDU_INFO_T for sending dvt RTS Frame.\n");
		return -1;
	}

	kalMemZero(prMsduInfo->prPacket, u2EstimatedFrameLen);

	prTxFrame = prMsduInfo->prPacket;

	/* Fill frame ctrl */
	prTxFrame->u2FrameCtrl = MAC_FRAME_RTS;
	duration = 192 + (uint16_t)((sizeof(struct _FRAME_RTS)<<4)/2);
	if (((sizeof(struct _FRAME_RTS)) << 4)%2)
		duration++;
	prTxFrame->u2Duration = 16 + (uint16_t)duration;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);

	/* Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
			prMsduInfo,
			prBssInfo->ucBssIndex,
			prStaRec->ucIndex,
			16, sizeof(struct _FRAME_RTS),
			pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	/* Enqueue the frame to send this control frame */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
	DBGLOG(REQ, INFO, "RTS - Send RTS\n");
	return WLAN_STATUS_SUCCESS;
}

/*
* This routine is used to test TXS function.
* Send BA packet
*/
int SendBA(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	struct MSDU_INFO *prMsduInfo;
	struct _FRAME_BA *prTxFrame;
	struct BSS_INFO *prBssInfo;
	uint16_t u2EstimatedFrameLen;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	ASSERT(prBssInfo);

	/* Calculate MSDU buffer length */
	u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD
		+ sizeof(struct _FRAME_BA);

	/* Alloc MSDU_INFO */
	prMsduInfo = (struct MSDU_INFO *)
		cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);

	if (!prMsduInfo) {
		DBGLOG(REQ, WARN,
			"No MSDU_INFO_T for sending dvt RTS Frame.\n");
		return -1;
	}

	kalMemZero(prMsduInfo->prPacket, u2EstimatedFrameLen);

	prTxFrame = prMsduInfo->prPacket;

	/* Fill frame ctrl */
	prTxFrame->u2FrameCtrl = MAC_FRAME_BLOCK_ACK;

	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);

	/* Compose the frame body's frame */
	prTxFrame->BarControl.ACKPolicy = 1;
	prTxFrame->BarControl.Compressed = 1;
	/* prTxFrame->StartingSeq.field.StartSeq = pBAEntry->LastIndSeq; */

	/* Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
			prMsduInfo,
			prBssInfo->ucBssIndex,
			prStaRec->ucIndex,
			16, sizeof(struct _FRAME_BA),
			pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

	/* Enqueue the frame to send this control frame */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
	DBGLOG(REQ, INFO, "BA - Send BA\n");
	return WLAN_STATUS_SUCCESS;
}

bool send_add_txs_queue(uint8_t pid, uint8_t wlan_idx)
{
	struct TXS_LIST *list = &automation_dvt.txs.txs_list;
	uint32_t idx = 0;
	unsigned long IrqFlags = 0;
	struct TXS_LIST_ENTRY *pEntry;
	struct TXS_LIST_ENTRY *pheadEntry;

	automation_dvt.txs.total_req++;

	if (!list || !automation_dvt.txs.isinit) {
		DBGLOG(REQ, WARN, "txs_list doesnot init\n");
		return FALSE;
	}

	spin_lock_irqsave(&list->lock, IrqFlags);

	pEntry = GetTxsEntryFromFreeList();

	if (!pEntry) {
		spin_unlock_irqrestore(&list->lock, IrqFlags);
		DBGLOG(REQ, LOUD, "pEntry is null!!!\n");
		return FALSE;
	}

	idx = automation_dvt.txs.pid % PID_SIZE;
	pheadEntry = &list->pHead[idx];
	pEntry->wlan_idx = wlan_idx;
	list_add(&pEntry->mList, &pheadEntry->mList);
	list->Num++;
	automation_dvt.txs.pid++;

	spin_unlock_irqrestore(&list->lock, IrqFlags);

	return TRUE;
}

bool receive_del_txs_queue(
	uint32_t sn,
	uint8_t pid,
	uint8_t wlan_idx,
	uint32_t time_stamp)
{
	struct TXS_LIST *list = &automation_dvt.txs.txs_list;
	unsigned long IrqFlags = 0;
	unsigned long IrqFlags2 = 0;
	struct TXS_FREE_LIST_POOL *pFreeEntrylist = NULL;
	struct TXS_LIST_ENTRY *pheadEntry = NULL;
	struct TXS_LIST_ENTRY *pEntry = NULL;

	automation_dvt.txs.total_rsp++;

	if (!list || !automation_dvt.txs.isinit) {
		DBGLOG(REQ, LOUD, "txs_list doesnot init\n");
		return FALSE;
	}

	pFreeEntrylist = list->pFreeEntrylist;
	spin_lock_irqsave(&list->lock, IrqFlags);

	list_for_each_entry(pEntry, &list->pHead[pid].mList, mList) {
		if (pEntry->wlan_idx == wlan_idx) {
			if (automation_dvt.txs.check_item[pid].time_stamp
				== time_stamp)
				automation_dvt.txs.duplicate_txs = TRUE;

			automation_dvt.txs.check_item[pid].time_stamp =
				time_stamp;
			list_del_init(&pEntry->mList);
			list->Num--;
			spin_lock_irqsave(&pFreeEntrylist->Lock, IrqFlags2);
			pheadEntry = &pFreeEntrylist->head;
			list_add_tail(&pEntry->mList, &pheadEntry->mList);
			pFreeEntrylist->entry_number += 1;
			spin_unlock_irqrestore(&pFreeEntrylist->Lock,
				IrqFlags2);
			break;
		}
	}
	spin_unlock_irqrestore(&list->lock, IrqFlags);
	return pEntry;
}

/*
* This routine is used to test TXS function.
* Send specific type of packet and check if TXS is back
*/
int priv_driver_txs_test(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct STA_RECORD *prStaRec = NULL;
	uint32_t u4WCID;
	uint8_t ucStaIdx;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int32_t i4Recv = 0;
	int32_t txs_test_type;
	int32_t txs_test_format;
	int8_t *this_char = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	this_char = kalStrStr(*apcArgv, "=");
	if (!this_char)
		return -1;
	this_char++;

	DBGLOG(REQ, LOUD, "string = %s\n", this_char);

	i4Recv = sscanf(this_char, "%d-%d-%d",
		&(txs_test_type), &(txs_test_format), &(u4WCID));

	DBGLOG(REQ, LOUD, "txs_test_type=%d, txs_test_format=%d, u4WCID=%d\n",
		txs_test_type, txs_test_format, u4WCID);

	if (!AutomationInit(prAdapter, TXS)) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"AutomationInit Fail!\n");
		return i4BytesWritten;
	}

	automation_dvt.txs.duplicate_txs = FALSE;

	switch (txs_test_type) {
	case TXS_INIT:
		TxsExit();
		break;

	case TXS_COUNT_TEST:
		automation_dvt.txs.stop_send_test = FALSE;
		automation_dvt.txs.test_type = TXS_COUNT_TEST;
		automation_dvt.txs.format = txs_test_format;
		break;

	case TXS_BAR_TEST:
		automation_dvt.txs.stop_send_test = FALSE;
		automation_dvt.txs.test_type = TXS_BAR_TEST;
		automation_dvt.txs.format = txs_test_format;
		/* SendRefreshBAR(pAd, pEntry); */
		break;

	case TXS_DEAUTH_TEST:
		automation_dvt.txs.stop_send_test = FALSE;
		automation_dvt.txs.test_type = TXS_DEAUTH_TEST;
		automation_dvt.txs.format = txs_test_format;
		/* aisFsmSteps(prAdapter, AIS_STATE_DISCONNECTING); */
		authSendDeauthFrame(prAdapter,
			prAdapter->prAisBssInfo,
			prAdapter->prAisBssInfo->prStaRecOfAP,
			(struct SW_RFB *) NULL,
			REASON_CODE_DEAUTH_LEAVING_BSS,
			aisDeauthXmitComplete);
		break;

	case TXS_RTS_TEST:
		automation_dvt.txs.stop_send_test = FALSE;
		automation_dvt.txs.test_type = TXS_RTS_TEST;
		automation_dvt.txs.format = txs_test_format;
		if (wlanGetStaIdxByWlanIdx(prAdapter, u4WCID, &ucStaIdx) ==
		WLAN_STATUS_SUCCESS) {
		prStaRec = &prAdapter->arStaRec[ucStaIdx];
		} else {
			DBGLOG(REQ, LOUD,
				"automation wlanGetStaIdxByWlanIdx failed\n");
		}
		SendRTS(prAdapter, prStaRec, AutomationTxDone);
		break;

	case TXS_BA_TEST:
		automation_dvt.txs.stop_send_test = FALSE;
		automation_dvt.txs.test_type = TXS_BA_TEST;
		automation_dvt.txs.format = txs_test_format;

		if (wlanGetStaIdxByWlanIdx(prAdapter, u4WCID, &ucStaIdx) ==
		WLAN_STATUS_SUCCESS) {
		prStaRec = &prAdapter->arStaRec[ucStaIdx];
		} else {
			DBGLOG(REQ, LOUD,
				"Automation wlanGetStaIdxByWlanIdx failed\n");
		}

		SendBA(prAdapter, prStaRec, AutomationTxDone);
		break;

	case TXS_DUMP_DATA:
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"automation_dvt.txs.test_type=%u\n",
			automation_dvt.txs.test_type);
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"automation_dvt.txs.format=%u\n",
			automation_dvt.txs.format);
		break;
	}

	return i4BytesWritten;
}

/*
* This routine is used to test TXS function.
* Check TXS test result
*/
int priv_driver_txs_test_result(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t txs_test_result = 0, wait_cnt = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int32_t i4Recv = 0;
	int8_t *this_char = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	struct TXS_LIST *list = NULL;


	list = &automation_dvt.txs.txs_list;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	this_char = kalStrStr(*apcArgv, "=");
	if (!this_char)
		return -1;
	this_char++;

	DBGLOG(REQ, LOUD, "string = %s\n", this_char);

	i4Recv = kalkStrtos32(this_char, 0, &(txs_test_result));
	DBGLOG(REQ, LOUD, "txs_test_result = %d\n", txs_test_result);

	if (!AutomationInit(prAdapter, TXS)) {
		DBGLOG(REQ, LOUD, "AutomationInit Fail!\n");
		return FALSE;
	}

	automation_dvt.txs.stop_send_test = TRUE;
	DBGLOG(REQ, LOUD, "wait entry to be deleted txs.total_req/rsp=%d %d\n",
		automation_dvt.txs.total_req, automation_dvt.txs.total_rsp);

	if (txs_test_result == 1) {
		while (automation_dvt.txs.total_req !=
			automation_dvt.txs.total_rsp) {
			kalMsleep(100);/* OS_WAIT(10); */
			wait_cnt++;
			if (wait_cnt > 100)
				break;
		}
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"txs.total_req %u\n", automation_dvt.txs.total_req);
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"txs.total_rsp %u\n", automation_dvt.txs.total_rsp);

		if (automation_dvt.txs.total_req == automation_dvt.txs.total_rsp
			 && (automation_dvt.txs.total_req != 0)) {
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				"TXS_COUNT_TEST------> PASS\n");
		} else {
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				"TXS_COUNT_TEST------> ERROR\n");
		}
	} else if (txs_test_result == 2) {
		while (list->Num > 0) {
			DBGLOG(REQ, LOUD, "wait entry to be deleted\n");
			kalMsleep(100);/* OS_WAIT(10);*/
			wait_cnt++;
			if (wait_cnt > 100)
				break;
		}

		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"txs.total_req %u\n", automation_dvt.txs.total_req);
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"txs.total_rsp %u\n", automation_dvt.txs.total_rsp);

		if (list->Num == 0) {
			if ((automation_dvt.txs.duplicate_txs == FALSE) &&
				(automation_dvt.txs.total_req != 0)) {
				LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
					"Correct Frame Test------> PASS\n");
			} else {
				LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
					"Correct Frame Test------> FAIL duplicate txs");
			}
		} else {
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				"Correct Frame Test------> FAIL  txs_q->Num = (%d)\n",
				list->Num);
		}
	}
	return i4BytesWritten;
}

/*
* return 0 : No Need Test
* 1: Check Data frame
* 2: Check management and control frame
*/
int is_frame_test(struct ADAPTER *pAd, uint8_t send_received)
{
	if (!pAd || (pAd->auto_dvt == NULL))
		return 0;

	if (send_received == 0 && automation_dvt.txs.stop_send_test == TRUE)
		return 0;

	switch (automation_dvt.txs.test_type) {
	case TXS_COUNT_TEST:
		return 1;
	case TXS_BAR_TEST:
	case TXS_DEAUTH_TEST:
	case TXS_RTS_TEST:
	case TXS_BA_TEST:
		return 2;

	default:
		return 0;
	}
}

uint32_t AutomationTxDone(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	IN enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	DBGLOG(REQ, LOUD, "AutomationTxDone!\n");
	if (rTxDoneStatus)
		DBGLOG(REQ, LOUD,
			"EVENT-TX DONE [status: %d][seq: %d]: Current Time = %d\n",
			rTxDoneStatus, prMsduInfo->ucTxSeqNum,
			kalGetTimeTick());
	return WLAN_STATUS_SUCCESS;
}


/*
* This routine is used to test RXV function.
* step1. AP fixed rate and ping to STA
* step2. STA iwpriv cmd with RXV_TEST=enable-TxMode-BW-MCS-SGI-STBC-LDPC
* step3. STA RXV_RESULT=1, check whether RX packets received from AP
*        matched with specific rate
*/
int priv_driver_rxv_test(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int32_t i4Recv = 0;
	uint32_t u4Enable = 0;
	int8_t *this_char = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint32_t u4Mode = 0, u4Bw = 0, u4Mcs = 0;
	uint32_t u4SGI = 0, u4STBC = 0, u4LDPC = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	this_char = kalStrStr(*apcArgv, "=");
	if (!this_char)
		return -1;
	this_char++;

	DBGLOG(REQ, LOUD, "string = %s\n", this_char);

	i4Recv = sscanf(this_char, "%d-%d-%d-%d-%d-%d-%d", &(u4Enable),
				&(u4Mode), &(u4Bw), &(u4Mcs),
				&(u4SGI), &(u4STBC), &(u4LDPC));
	DBGLOG(RX, LOUD,
		"%s():Enable = %d, Mode = %d, BW = %d, MCS = %d\n"
		"\t\t\t\tSGI = %d, STBC = %d, LDPC = %d\n",
		__func__, u4Enable, u4Mode, u4Bw, u4Mcs,
		u4SGI, u4STBC, u4LDPC);

	if (!AutomationInit(prAdapter, RXV)) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"AutomationInit Fail!\n");
		return i4BytesWritten;
	}

	if (u4Mode == TX_RATE_MODE_OFDM) {
		switch (u4Mcs) {
		case 0:
			u4Mcs = 11;
			break;
		case 1:
			u4Mcs = 15;
			break;
		case 2:
			u4Mcs = 10;
			break;
		case 3:
			u4Mcs = 14;
			break;
		case 4:
			u4Mcs = 9;
			break;
		case 5:
			u4Mcs = 13;
			break;
		case 6:
			u4Mcs = 8;
			break;
		case 7:
			u4Mcs = 12;
			break;
		default:
			DBGLOG(RX, ERROR,
				"[%s]OFDM mode but wrong MCS!\n", __func__);
			break;
		}
	}

	automation_dvt.rxv.rxv_test_result = TRUE;
	automation_dvt.rxv.enable = u4Enable;
	automation_dvt.rxv.rx_count = 0;

	/* expected packets */
	automation_dvt.rxv.rx_mode = u4Mode;
	automation_dvt.rxv.rx_bw = u4Bw;
	automation_dvt.rxv.rx_rate = u4Mcs;
	automation_dvt.rxv.rx_sgi = u4SGI;
	automation_dvt.rxv.rx_stbc = u4STBC;
	automation_dvt.rxv.rx_ldpc = u4LDPC;

	return i4BytesWritten;
}

/*
* This routine is used to judge result of RXV DVT.
*/
int priv_driver_rxv_test_result(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"RXV Test------> rx_count(%d)\n",
			automation_dvt.rxv.rx_count);

	if (automation_dvt.rxv.rxv_test_result == TRUE &&
		automation_dvt.rxv.rx_count != 0) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"RXV Test------> PASS\n");
	} else {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"RXV Test------> FAIL\n");
	}

	automation_dvt.rxv.enable = 0;

	return i4BytesWritten;
}

#if (CFG_SUPPORT_CONNAC2X == 1)
/*
* This routine is used to test RXV function.
* It will check RXV of incoming packets if match pre-setting from iwpriv
* Note. This is FALCON RXV format
*/
void connac2x_rxv_correct_test(
	IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb)
{
	uint32_t prxv1, crxv1;
	uint32_t txmode, rate, frmode, sgi, nsts, ldpc, stbc;

	automation_dvt.rxv.rx_count++;

	/* P-RXV1 */
	prxv1 = ((struct HW_MAC_RX_STS_GROUP_3_V2 *)
				prSwRfb->prRxStatusGroup3)->u4RxVector[0];
	rate = (prxv1 & CONNAC2X_RX_VT_RX_RATE_MASK)
				>> CONNAC2X_RX_VT_RX_RATE_OFFSET;
	nsts = (prxv1 & CONNAC2X_RX_VT_NSTS_MASK)
				>> CONNAC2X_RX_VT_NSTS_OFFSET;
	ldpc = prxv1 & CONNAC2X_RX_VT_LDPC;
	/* C-RXV1 */
	crxv1 = prSwRfb->prRxStatusGroup5->u4RxVector[0];
	stbc = (crxv1 & CONNAC2X_RX_VT_STBC_MASK)
				>> CONNAC2X_RX_VT_STBC_OFFSET;
	txmode = (crxv1 & CONNAC2X_RX_VT_RX_MODE_MASK)
				>> CONNAC2X_RX_VT_RX_MODE_OFFSET;
	frmode = (crxv1 & CONNAC2X_RX_VT_FR_MODE_MASK)
				>> CONNAC2X_RX_VT_FR_MODE_OFFSET;
	sgi = (crxv1 & CONNAC2X_RX_VT_SHORT_GI_MASK)
				>> CONNAC2X_RX_VT_SHORT_GI_OFFSET;

	if (txmode != automation_dvt.rxv.rx_mode) {
		automation_dvt.rxv.rxv_test_result = FALSE;
		DBGLOG(RX, ERROR, "[%s]Receive TxMode=%d, Check RxMode=%d\n",
		__func__, txmode, automation_dvt.rxv.rx_mode);
	}
	if (rate != automation_dvt.rxv.rx_rate) {
		automation_dvt.rxv.rxv_test_result = FALSE;
		DBGLOG(RX, ERROR, "[%s]Receive TxRate=%d, Check RxRate=%d\n",
		__func__, rate, automation_dvt.rxv.rx_rate);
	}
	if (frmode != automation_dvt.rxv.rx_bw) {
		automation_dvt.rxv.rxv_test_result = FALSE;
		DBGLOG(RX, ERROR, "[%s]Receive BW=%d, Check BW=%d\n",
		__func__, frmode, automation_dvt.rxv.rx_bw);
	}
	if (sgi != automation_dvt.rxv.rx_sgi) {
		automation_dvt.rxv.rxv_test_result = FALSE;
		DBGLOG(RX, ERROR, "[%s]Receive Sgi=%d, Check Sgi=%d\n",
		__func__, sgi, automation_dvt.rxv.rx_sgi);
	}
	if (stbc != automation_dvt.rxv.rx_stbc) {
		automation_dvt.rxv.rxv_test_result = FALSE;
		DBGLOG(RX, ERROR, "[%s]Receive Stbc=%d, Check Stbc=%d\n",
		__func__, stbc, automation_dvt.rxv.rx_stbc);
	}
	if (ldpc != automation_dvt.rxv.rx_ldpc) {
		automation_dvt.rxv.rxv_test_result = FALSE;
		DBGLOG(RX, ERROR, "[%s]Receive Ldpc=%d, Check Ldpc=%d\n",
		__func__, ldpc, automation_dvt.rxv.rx_ldpc);
	}

	DBGLOG(RX, INFO,
	"\n================ RXV Automation end ================\n");
}
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
/*
* This routine is used to test CSO function.
* Set 0xffff at checksum filed when cso_ctrl is enabled(15 for all TX case)
* Checksum should be recalculated by HW CSO function.
* step1. iwpriv cmd with CSO_TEST=15 (set CRC of tx packet to 0xffff)
* step2. run TX traffic
* step3. Passed if throughput is normal
*/
int priv_driver_cso_test(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int32_t i4Recv = 0;
	uint8_t ucCsoCtrl = 0;
	int8_t *this_char = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	this_char = kalStrStr(*apcArgv, "=");
	if (!this_char)
		return -1;
	this_char++;

	DBGLOG(REQ, LOUD, "string = %s\n", this_char);

	i4Recv = kalkStrtou8(this_char, 0, &(ucCsoCtrl));
	DBGLOG(RX, LOUD, "cso_ctrl = %u\n", ucCsoCtrl);

	if (!AutomationInit(prAdapter, CSO)) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"AutomationInit Fail!\n");
		return i4BytesWritten;
	}

	/* CSO_TEST=15 for all TX test case                  */
	/* CSO_TX_IPV4 = BIT(0),                             */
	/* CSO_TX_IPV6 = BIT(1),                             */
	/* CSO_TX_TCP = BIT(2),                              */
	/* CSO_TX_UDP = BIT(3),                              */
	automation_dvt.cso_ctrl = ucCsoCtrl;

	return i4BytesWritten;
}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

/*
* This routine is used to test HW feature
* Set a value to allow how many packets be transmitting
*/
int priv_driver_set_tx_test(
			IN struct net_device *prNetDev, IN char *pcCommand,
			IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint32_t u4Ret, u4Parse;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		prAdapter->u2TxTest = (uint16_t) u4Parse;
		DBGLOG(REQ, LOUD, "prAdapter->u2TxTest = %d\n",
			prAdapter->u2TxTest);
	} else {
		DBGLOG(REQ, ERROR, "iwpriv wlanXX driver TX_TEST xxxx\n");
	}

	prAdapter->u2TxTestCount = 0;
	return i4BytesWritten;
}

/*
* This routine is used to test HE Trigger Data
* Assign specific AC of Data to verify HW behavior when receive Trigger frame
*/
int priv_driver_set_tx_test_ac(
			IN struct net_device *prNetDev, IN char *pcCommand,
			IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint32_t u4Ret, u4Parse;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		prAdapter->ucTxTestUP = (uint8_t) u4Parse;
		DBGLOG(REQ, LOUD, "prAdapter->ucTxTestUP = %d\n",
			prAdapter->ucTxTestUP);
	} else {
		DBGLOG(REQ, ERROR, "iwpriv wlanXX driver TX_TEST_AC xx\n");
	}

	return i4BytesWritten;
}

/*
* This routine is used to skip legal channel sanity check.
* During FPGA stage, could get wrong frequency information
* from RXD. Ignore this error
*/
int priv_driver_skip_legal_ch_check(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int32_t i4Recv = 0;
	uint32_t u4Enable = 0;
	int8_t *this_char = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	this_char = kalStrStr(*apcArgv, "=");
	if (!this_char)
		return -1;
	this_char++;

	DBGLOG(REQ, LOUD, "string = %s\n", this_char);

	i4Recv = kalkStrtos32(this_char, 0, &(u4Enable));
	DBGLOG(RX, LOUD, "skip_legal_ch_enable = %d\n", u4Enable);

	if (!AutomationInit(prAdapter, SKIP_CH)) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"AutomationInit Fail!\n");
		return i4BytesWritten;
	}

	automation_dvt.skip_legal_ch_enable = u4Enable;
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				"skip_legal_ch_enable = %d\n", u4Enable);
	return i4BytesWritten;
}

#endif /* CFG_SUPPORT_WIFI_SYSDVT */

