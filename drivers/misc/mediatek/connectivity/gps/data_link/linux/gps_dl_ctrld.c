/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include "gps_dl_ctrld.h"
#include "gps_each_device.h"
#if GPS_DL_MOCK_HAL
#include "gps_mock_mvcd.h"
#endif
#include "gps_data_link_devices.h"
#include "gps_dl_hal_api.h"

struct gps_dl_ctrld_context gps_dl_ctrld;

static int gps_dl_opfunc_link_event_proc(struct gps_dl_osal_op_dat *pOpDat);
static int gps_dl_opfunc_hal_event_proc(struct gps_dl_osal_op_dat *pOpDat);
static struct gps_dl_osal_lxop *gps_dl_get_op(struct gps_dl_osal_lxop_q *pOpQ);
static int gps_dl_put_op(struct gps_dl_osal_lxop_q *pOpQ, struct gps_dl_osal_lxop *pOp);

static const GPS_DL_OPID_FUNC gps_dl_core_opfunc[] = {
	[GPS_DL_OPID_LINK_EVENT_PROC] = gps_dl_opfunc_link_event_proc,
	[GPS_DL_OPID_HAL_EVENT_PROC]  = gps_dl_opfunc_hal_event_proc,
};

static int gps_dl_opfunc_link_event_proc(struct gps_dl_osal_op_dat *pOpDat)
{
	enum gps_dl_link_event_id evt;
	enum gps_dl_link_id_enum link_id;

	link_id = (enum gps_dl_link_id_enum)pOpDat->au4OpData[0];
	evt = (enum gps_dl_link_event_id)pOpDat->au4OpData[1];
	gps_dl_link_event_proc(evt, link_id);

	return 0;
}

static int gps_dl_opfunc_hal_event_proc(struct gps_dl_osal_op_dat *pOpDat)
{
	enum gps_dl_hal_event_id evt;
	enum gps_dl_link_id_enum link_id;
	int sid_on_evt;

	link_id = (enum gps_dl_link_id_enum)pOpDat->au4OpData[0];
	evt = (enum gps_dl_hal_event_id)pOpDat->au4OpData[1];
	sid_on_evt = (int)pOpDat->au4OpData[2];
	gps_dl_hal_event_proc(evt, link_id, sid_on_evt);

	return 0;
}

unsigned int gps_dl_wait_event_checker(struct gps_dl_osal_thread *pThread)
{
	struct gps_dl_ctrld_context *pgps_dl_ctrld;

	if (pThread) {
		pgps_dl_ctrld = (struct gps_dl_ctrld_context *) (pThread->pThreadData);
		return !RB_EMPTY(&pgps_dl_ctrld->rOpQ);
	}
	GDL_LOGE_EVT("pThread null");
	return 0;
}

static int gps_dl_core_opid(struct gps_dl_osal_op_dat *pOpDat)
{
	int ret;

	if (pOpDat == NULL) {
		GDL_LOGE_EVT("null operation data");
		/*print some message with error info */
		return -1;
	}

	if (pOpDat->opId >= GPS_DL_OPID_MAX) {
		GDL_LOGE_EVT("Invalid OPID(%d)", pOpDat->opId);
		return -2;
	}

	if (gps_dl_core_opfunc[pOpDat->opId]) {
		GDL_LOGD_EVT("GPS data link: operation id(%d)", pOpDat->opId);
		ret = (*(gps_dl_core_opfunc[pOpDat->opId])) (pOpDat);
		return ret;
	}

	GDL_LOGE_EVT("GPS data link: null handler (%d)", pOpDat->opId);

	return -2;
}

static int gps_dl_put_op(struct gps_dl_osal_lxop_q *pOpQ, struct gps_dl_osal_lxop *pOp)
{
	int iRet;

	if (!pOpQ || !pOp) {
		GDL_LOGW_EVT("invalid input param: pOpQ(0x%p), pLxOp(0x%p)", pOpQ, pOp);
		gps_dl_osal_assert(pOpQ);
		gps_dl_osal_assert(pOp);
		return -1;
	}

	iRet = gps_dl_osal_lock_sleepable_lock(&pOpQ->sLock);
	if (iRet) {
		GDL_LOGW_EVT("gps_dl_osal_lock_sleepable_lock iRet(%d)", iRet);
		return -1;
	}

	/* acquire lock success */
	if (!RB_FULL(pOpQ))
		RB_PUT(pOpQ, pOp);
	else {
		GDL_LOGW("RB_FULL(%p -> %p)", pOp, pOpQ);
		iRet = -1;
	}

	gps_dl_osal_unlock_sleepable_lock(&pOpQ->sLock);

	if (iRet)
		return -1;
	return 0;
}

int gps_dl_put_act_op(struct gps_dl_osal_lxop *pOp)
{
	struct gps_dl_ctrld_context *pgps_dl_ctrld = &gps_dl_ctrld;
	struct gps_dl_osal_signal *pSignal = NULL;
	int waitRet = -1;
	int bRet = 0;

	gps_dl_osal_assert(pgps_dl_ctrld);
	gps_dl_osal_assert(pOp);

	do {
		if (!pgps_dl_ctrld || !pOp) {
			GDL_LOGE("pgps_dl_ctx(0x%p), pOp(0x%p)", pgps_dl_ctrld, pOp);
			break;
		}

		/* Init ref_count to 1 indicating that current thread holds a ref to it */
		atomic_set(&pOp->ref_count, 1);

		pSignal = &pOp->signal;
		if (pSignal->timeoutValue) {
			pOp->result = -9;
			gps_dl_osal_signal_init(pSignal);
		}

		/* Increment ref_count by 1 as gps control thread will hold a reference also,
		 * this must be done here instead of on target thread, because
		 * target thread might not be scheduled until a much later time,
		 * allowing current thread to decrement ref_count at the end of function,
		 * putting op back to free queue before target thread has a chance to process.
		 */
		atomic_inc(&pOp->ref_count);

		/* put to active Q */
		bRet = gps_dl_put_op(&pgps_dl_ctrld->rOpQ, pOp);
		if (bRet == -1) {
			GDL_LOGE("put to active queue fail");
			atomic_dec(&pOp->ref_count);
			break;
		}

		/* wake up gps control thread */
		gps_dl_osal_trigger_event(&pgps_dl_ctrld->rgpsdlWq);

		if (pSignal->timeoutValue == 0) {
			bRet = -1;
			break;
		}

		/* check result */
		waitRet = gps_dl_osal_wait_for_signal_timeout(pSignal, &pgps_dl_ctrld->thread);

		if (waitRet == 0)
			GDL_LOGE("opId(%d) completion timeout", pOp->op.opId);
		else if (pOp->result)
			GDL_LOGW("opId(%d) result:%d", pOp->op.opId, pOp->result);

		/* op completes, check result */
		bRet = (pOp->result) ? -1 : 0;
	} while (0);

	if (pOp && atomic_dec_and_test(&pOp->ref_count)) {
		/* put Op back to freeQ */
		gps_dl_put_op(&pgps_dl_ctrld->rFreeOpQ, pOp);
	}

	return bRet;
}

struct gps_dl_osal_lxop *gps_dl_get_free_op(void)
{
	struct gps_dl_osal_lxop *pOp = NULL;
	struct gps_dl_ctrld_context *pgps_dl_ctrld = &gps_dl_ctrld;

	gps_dl_osal_assert(pgps_dl_ctrld);
	pOp = gps_dl_get_op(&pgps_dl_ctrld->rFreeOpQ);
	if (pOp)
		gps_dl_osal_memset(pOp, 0, sizeof(struct gps_dl_osal_lxop));
	return pOp;
}

static struct gps_dl_osal_lxop *gps_dl_get_op(struct gps_dl_osal_lxop_q *pOpQ)
{
	struct gps_dl_osal_lxop *pOp;
	int iRet;

	if (pOpQ == NULL) {
		GDL_LOGE("pOpQ = NULL");
		gps_dl_osal_assert(pOpQ);
		return NULL;
	}

	iRet = gps_dl_osal_lock_sleepable_lock(&pOpQ->sLock);
	if (iRet) {
		GDL_LOGE("gps_dl_osal_lock_sleepable_lock iRet(%d)", iRet);
		return NULL;
	}

	/* acquire lock success */
	RB_GET(pOpQ, pOp);
	gps_dl_osal_unlock_sleepable_lock(&pOpQ->sLock);

	if (pOp == NULL) {
		GDL_LOGW("RB_GET(%p) return NULL", pOpQ);
		gps_dl_osal_assert(pOp);
		return NULL;
	}

	return pOp;
}

int gps_dl_put_op_to_free_queue(struct gps_dl_osal_lxop *pOp)
{
	struct gps_dl_ctrld_context *pgps_dl_ctrld = &gps_dl_ctrld;

	if (gps_dl_put_op(&pgps_dl_ctrld->rFreeOpQ, pOp) < 0)
		return -1;

	return 0;
}

static int gps_dl_ctrl_thread(void *pData)
{
	struct gps_dl_ctrld_context *pgps_dl_ctrld = (struct gps_dl_ctrld_context *) pData;
	struct gps_dl_osal_event *pEvent = NULL;
	struct gps_dl_osal_lxop *pOp;
	int iResult;

	if (pgps_dl_ctrld == NULL) {
		GDL_LOGE("pgps_dl_ctx is NULL");
		return -1;
	}

	GDL_LOGI("gps control thread starts");

	pEvent = &(pgps_dl_ctrld->rgpsdlWq);

	for (;;) {
		pOp = NULL;
		pEvent->timeoutValue = 0;

		gps_dl_osal_thread_wait_for_event(&pgps_dl_ctrld->thread, pEvent, gps_dl_wait_event_checker);

		if (gps_dl_osal_thread_should_stop(&pgps_dl_ctrld->thread)) {
			GDL_LOGW(" thread should stop now...");
			/* TODO: clean up active opQ */
			break;
		}

		/* get Op from Queue */
		pOp = gps_dl_get_op(&pgps_dl_ctrld->rOpQ);
		if (!pOp) {
			GDL_LOGW("get_lxop activeQ fail");
			continue;
		}

		/*Execute operation*/
		iResult = gps_dl_core_opid(&pOp->op);

		if (atomic_dec_and_test(&pOp->ref_count))
			gps_dl_put_op(&pgps_dl_ctrld->rFreeOpQ, pOp);
		else if (gps_dl_osal_op_is_wait_for_signal(pOp))
			gps_dl_osal_op_raise_signal(pOp, iResult);

		if (iResult)
			GDL_LOGW("opid (0x%x) failed, iRet(%d)", pOp->op.opId, iResult);

	}

	GDL_LOGI("gps control thread exits succeed");

	return 0;
}

int gps_dl_ctrld_init(void)
{
	struct gps_dl_ctrld_context *pgps_dl_ctrld;
	struct gps_dl_osal_thread *pThread;
	int iRet;
	int i;

	pgps_dl_ctrld = &gps_dl_ctrld;
	gps_dl_osal_memset(&gps_dl_ctrld, 0, sizeof(gps_dl_ctrld));

	/* Create gps data link control thread */
	pThread = &gps_dl_ctrld.thread;
	gps_dl_osal_strncpy(pThread->threadName, "gps_kctrld", sizeof(pThread->threadName));
	pThread->pThreadData = (void *)pgps_dl_ctrld;
	pThread->pThreadFunc = (void *)gps_dl_ctrl_thread;

	iRet = gps_dl_osal_thread_create(pThread);
	if (iRet) {
		GDL_LOGE("Create gps data link control thread fail:%d", iRet);
		return -1;
	}

	/* Initialize gps control Thread Information: Thread */
	gps_dl_osal_event_init(&pgps_dl_ctrld->rgpsdlWq);
	gps_dl_osal_sleepable_lock_init(&pgps_dl_ctrld->rOpQ.sLock);
	gps_dl_osal_sleepable_lock_init(&pgps_dl_ctrld->rFreeOpQ.sLock);
	/* Initialize op queue */
	RB_INIT(&pgps_dl_ctrld->rOpQ, GPS_DL_OP_BUF_SIZE);
	RB_INIT(&pgps_dl_ctrld->rFreeOpQ, GPS_DL_OP_BUF_SIZE);

	/* Put all to free Q */
	for (i = 0; i < GPS_DL_OP_BUF_SIZE; i++) {
		gps_dl_osal_signal_init(&(pgps_dl_ctrld->arQue[i].signal));
		gps_dl_put_op(&pgps_dl_ctrld->rFreeOpQ, &(pgps_dl_ctrld->arQue[i]));
	}

	iRet = gps_dl_osal_thread_run(pThread);
	if (iRet) {
		GDL_LOGE("gps data link ontrol thread run fail:%d", iRet);
		return -1;
	}

	return 0;
}

int gps_dl_ctrld_deinit(void)
{
	struct gps_dl_osal_thread *pThread;
	int iRet;

	pThread = &gps_dl_ctrld.thread;

	iRet = gps_dl_osal_thread_stop(pThread);
	if (iRet)
		GDL_LOGE("gps data link ontrol thread stop fail:%d", iRet);
	else
		GDL_LOGI("gps data link ontrol thread stop okay:%d", iRet);

	gps_dl_osal_event_deinit(&gps_dl_ctrld.rgpsdlWq);

	iRet = gps_dl_osal_thread_destroy(pThread);
	if (iRet) {
		GDL_LOGE("gps data link ontrol thread destroy fail:%d", iRet);
		return -1;
	}
	GDL_LOGI("gps data link ontrol thread destroy okay:%d\n", iRet);

	return 0;
}

