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
/******************************************************************************
*[File]             sdio_ring.c
*[Version]          v1.0
*[Revision Date]    2018-09-03
*[Author]
*[Description]
*    The program provides SDIO HIF ring buffer architecture
*[Copyright]
*    Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
******************************************************************************/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_os.h"
#include "precomp.h"
/*******************************************************************************
*                              C O N S T A N T S
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

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static wait_queue_head_t waitq_sdio_ring;
static unsigned long ulFlag;
/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
int sdio_ring_thread(void *data)
{
	int ret;
	struct GL_HIF_INFO *prHif = NULL;
	uint32_t i;
	struct net_device *dev = data;
	struct GLUE_INFO *prGlueInfo = *((struct GLUE_INFO **)
					 netdev_priv(dev));

	set_user_nice(current,
		      prGlueInfo->prAdapter->rWifiVar.cThreadNice);
	DBGLOG(INIT, INFO, "%s:%u starts running...\n",
		KAL_GET_CURRENT_THREAD_NAME(),
		KAL_GET_CURRENT_THREAD_ID());

	while (TRUE) {
		if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
			DBGLOG(INIT, INFO,
				"%s:%u should stop now...\n",
				KAL_GET_CURRENT_THREAD_NAME(),
				KAL_GET_CURRENT_THREAD_ID());
			break;
		}
		do {
			ret = wait_event_interruptible(waitq_sdio_ring,
				((ulFlag & BIT(SDIO_RING_TX_BIT)) != 0));
		} while (ret != 0);
		if (test_and_clear_bit(SDIO_RING_TX_BIT, &ulFlag))
			nicTxMsduQueueMthread(prGlueInfo->prAdapter);
	}

	/* Resource destroy */
	prHif = &prGlueInfo->rHifInfo;
	for (i = 0; i < SDIO_RING_SIZE; i++) {
		kalReleaseIOBuffer(prHif->ring_buffer[i],
					SDIO_RING_BUF_LEN);
		prHif->ring_buffer[i] = NULL;
	}
	prHif->sdio_ring_thread = NULL;

	DBGLOG(INIT, INFO, "%s:%u stopped!\n",
	       KAL_GET_CURRENT_THREAD_NAME(),
	       KAL_GET_CURRENT_THREAD_ID());

	return 0;
}

uint32_t Initial_sdio_ring(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter;
	struct GL_HIF_INFO *prHif = NULL;
	uint32_t i;

	prAdapter = prGlueInfo->prAdapter;
	prHif = &prGlueInfo->rHifInfo;

	for (i = 0; i < SDIO_RING_SIZE; i++) {
		prHif->ring_buffer[i] = (uint8_t *)kalAllocateIOBuffer(
				SDIO_RING_BUF_LEN);
		if (prHif->ring_buffer[i] == NULL) {
			DBGLOG(INIT, ERROR,
			       "Could not allocate %u bytes for SDIO ring buffer[%u].\n",
			       SDIO_RING_BUF_LEN, i);
			return FALSE;
		}
	}
	kalMemSet(&prHif->ring_len, 0, sizeof(uint32_t)*SDIO_RING_SIZE);
	kalMemSet(&prHif->ring_PSE, 0, sizeof(uint32_t)*SDIO_RING_SIZE);
	kalMemSet(&prHif->ring_PLE, 0, sizeof(uint32_t)*SDIO_RING_SIZE);
	prHif->ring_head = 0;
	prHif->ring_tail = 0;
	ulFlag = 0;
	prAdapter->rQM.fgTcResourcePostHandle = 1;
	init_waitqueue_head(&waitq_sdio_ring);

	prHif->sdio_ring_thread = kthread_run(sdio_ring_thread,
				prGlueInfo->prDevHandler, "sdio_ring_thread");
	if (prGlueInfo->prAdapter->rWifiVar.ucThreadPriority > 0) {
		struct sched_param param = {
			.sched_priority = prGlueInfo->prAdapter
			->rWifiVar.ucThreadPriority
		};
		sched_setscheduler(prHif->sdio_ring_thread,
						prGlueInfo->prAdapter->rWifiVar
						.ucThreadScheduling, &param);
	}

	return TRUE;
}
uint8_t sdio_ring_push(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter;
	struct GL_HIF_INFO *prHif = NULL;
	uint32_t head;
	uint32_t tail;
	uint32_t data_len;
	uint8_t *buf;

	prAdapter = prGlueInfo->prAdapter;
	prHif = &prGlueInfo->rHifInfo;

	head = prHif->ring_head;
	tail = prHif->ring_tail;
	buf = prHif->ring_buffer[head];
	data_len = prHif->ring_len[head];



	if (((head + 1) % SDIO_RING_SIZE) != tail) {
		if ((SDIO_RING_BUF_LEN - ALIGN_4(data_len)) >=
			HIF_TX_TERMINATOR_LEN) {
			/* fill with single dword of zero as */
			/* TX-aggregation termination */
			*(uint32_t *) (&((buf)[ALIGN_4(data_len)])) = 0;
		}

		head = ((head + 1) % SDIO_RING_SIZE);
		prHif->ring_head = head;

		return TRUE;
	}

	return FALSE;
}
uint8_t sdio_ring_pop(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter;
	struct GL_HIF_INFO *prHif = NULL;
	uint32_t head;
	uint32_t tail;

	prAdapter = prGlueInfo->prAdapter;
	prHif = &prGlueInfo->rHifInfo;

	head = prHif->ring_head;
	tail = prHif->ring_tail;



	if (tail != head) {
		tail = ((tail + 1) % SDIO_RING_SIZE);
		prHif->ring_tail = tail;

		return TRUE;
	}

	return FALSE;
}
static u_int8_t
Acquire_All_Resource(struct ADAPTER *prAdapter,
u_int32_t PLE_cnt, u_int32_t PSE_cnt)
{
	uint8_t i;
	u_int32_t total_PSE, total_PLE;
	struct TX_CTRL *prTxCtrl;
	struct TX_TCQ_STATUS *prTc;
	struct QUE_MGT *prQM;

	KAL_SPIN_LOCK_DECLARATION();

	prTxCtrl = &prAdapter->rTxCtrl;
	prTc = &prTxCtrl->rTc;
	prQM = &prAdapter->rQM;

	total_PSE = 0;
	total_PLE = 0;
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
	for (i = 0; i < 4; i++) {
		total_PSE += prTc->au4FreePageCount[i];
		total_PLE += prTc->au4FreePageCount_PLE[i];
	}

	if ((total_PSE < PSE_cnt) || (total_PLE < PLE_cnt)) {
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
		return FALSE;
	}

	for (i = 0; i < 4; i++) {
		if ((PLE_cnt != 0) || (PSE_cnt != 0)) {
			/* PSE part */
			if (prTc->au4FreePageCount[i] >= PSE_cnt) {
				prTc->au4FreePageCount[i] -= PSE_cnt;
				prQM->au4QmTcUsedPageCounter[i] += PSE_cnt;
				PSE_cnt = 0;
			} else {
				PSE_cnt -= prTc->au4FreePageCount[i];
				prQM->au4QmTcUsedPageCounter[i] +=
					prTc->au4FreePageCount[i];
				prTc->au4FreePageCount[i] = 0;
			}
			prTc->au4FreeBufferCount[i] =
				(prTc->au4FreePageCount[i] /
				(prAdapter->rTxCtrl.u4MaxPageCntPerFrame));

			/* PLE part */
			if (prTc->au4FreePageCount_PLE[i] >= PLE_cnt) {
				prTc->au4FreePageCount_PLE[i] -= PLE_cnt;
				PLE_cnt = 0;
			} else {
				PLE_cnt -= prTc->au4FreePageCount_PLE[i];
				prTc->au4FreePageCount_PLE[i] = 0;
			}

		} else {
			break;
		}
	}
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

	return TRUE;
}
void Set_sdio_ring_event(void)
{
	set_bit(SDIO_RING_TX_BIT, &ulFlag);
	wake_up_interruptible(&waitq_sdio_ring);
}
uint32_t sdio_ring_kcik_data(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter;
	struct GL_HIF_INFO *prHif = NULL;
	uint32_t head;
	uint32_t tail;
	uint32_t data_len;
	uint32_t req_PSE;
	uint32_t req_PLE;
	uint8_t *buf;

	prAdapter = prGlueInfo->prAdapter;
	prHif = &prGlueInfo->rHifInfo;

	head = prHif->ring_head;
	tail = prHif->ring_tail;

	if (tail != head) {
		buf = prHif->ring_buffer[tail];
		data_len = prHif->ring_len[tail];
		req_PSE = prHif->ring_PSE[tail];
		req_PLE = prHif->ring_PLE[tail];
		if (Acquire_All_Resource(prAdapter, req_PLE, req_PSE) == TRUE) {
			if (kalDevPortWrite(prGlueInfo, MCR_WTDR1, data_len,
				buf, SDIO_RING_BUF_LEN) == FALSE) {
				HAL_SET_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR);
				fgIsBusAccessFailed = TRUE;
				return FALSE;
			}

			prHif->ring_len[tail] = 0;
			prHif->ring_PSE[tail] = 0;
			prHif->ring_PLE[tail] = 0;

			sdio_ring_pop(prGlueInfo);

			kalSetTxEvent2Hif(prGlueInfo);
			Set_sdio_ring_event();

		} else {
			return FALSE;
		}
	}
	return TRUE;
}
