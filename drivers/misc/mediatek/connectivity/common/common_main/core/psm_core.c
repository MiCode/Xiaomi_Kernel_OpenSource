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

#include "osal_typedef.h"
#include "osal.h"
#include "psm_core.h"
#include "stp_core.h"
#include "stp_dbg.h"
#include "wmt_detect.h"
#include "wmt_exp.h"
#include <mtk_wcn_cmb_stub.h>
#include <linux/timer.h>

INT32 gPsmDbgLevel = STP_PSM_LOG_INFO;
MTKSTP_PSM_T stp_psm_i;
MTKSTP_PSM_T *stp_psm = &stp_psm_i;

STP_PSM_RECORD_T *g_stp_psm_dbg;
static UINT32 g_record_num;

P_STP_PSM_OPID_RECORD g_stp_psm_opid_dbg;
static UINT32 g_opid_record_num;

static UINT32 stp_traffic_start;
static UINT32 stp_traffic_current;

#define STP_PSM_PR_LOUD(fmt, arg...) \
do { \
	if (gPsmDbgLevel >= STP_PSM_LOG_LOUD) \
		pr_info(PFX_PSM "%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_PSM_PR_DBG(fmt, arg...) \
do { \
	if (gPsmDbgLevel >= STP_PSM_LOG_DBG) \
		pr_info(PFX_PSM "%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_PSM_PR_INFO(fmt, arg...) \
do { \
	if (gPsmDbgLevel >= STP_PSM_LOG_INFO) \
		pr_info(PFX_PSM "[I]%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_PSM_PR_WARN(fmt, arg...) \
do { \
	if (gPsmDbgLevel >= STP_PSM_LOG_WARN) \
		pr_warn(PFX_PSM "[W]%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_PSM_PR_ERR(fmt, arg...) \
do { \
	if (gPsmDbgLevel >= STP_PSM_LOG_ERR) \
		pr_err(PFX_PSM "[E]%s(%d):ERROR! "   fmt, __func__, __LINE__, ##arg); \
} while (0)


static inline INT32 _stp_psm_notify_wmt(MTKSTP_PSM_T *stp_psm, const MTKSTP_PSM_ACTION_T action);
static INT32 _stp_psm_thread_lock_aquire(MTKSTP_PSM_T *stp_psm);
static INT32 _stp_psm_thread_lock_release(MTKSTP_PSM_T *stp_psm);
static INT32 _stp_psm_dbg_dmp_in(STP_PSM_RECORD_T *stp_psm_dbg, UINT32 flag, UINT32 line_num);
static INT32 _stp_psm_dbg_out_printk(STP_PSM_RECORD_T *stp_psm_dbg);
static INT32 _stp_psm_opid_dbg_dmp_in(P_STP_PSM_OPID_RECORD p_opid_dbg, UINT32 opid, UINT32 line_num);
static INT32 _stp_psm_opid_dbg_out_printk(P_STP_PSM_OPID_RECORD p_opid_dbg);


static const PINT8 g_psm_state[STP_PSM_MAX_STATE] = {
	"ACT",
	"ACT_INACT",
	"INACT",
	"INACT_ACT"
};

static const PINT8 g_psm_action[STP_PSM_MAX_ACTION] = {
	"SLEEP",
	"HOST_AWAKE",
	"WAKEUP",
	"EIRQ",
	"ROLL_BACK"
};

static const PINT8 g_psm_op_name[STP_OPID_PSM_NUM] = {
	"STP_OPID_PSM_SLEEP",
	"STP_OPID_PSM_WAKEUP",
	"STP_OPID_PSM_HOST_AWAKE",
	"STP_OPID_PSM_EXIT"
};

static INT32 _stp_psm_release_data(MTKSTP_PSM_T *stp_psm);

static inline INT32 _stp_psm_get_state(MTKSTP_PSM_T *stp_psm);

static INT32 _stp_psm_is_redundant_active_op(P_OSAL_OP pOp, P_OSAL_OP_Q pOpQ);

static INT32 _stp_psm_clean_up_redundant_active_op(P_OSAL_OP_Q pOpQ);
static MTK_WCN_BOOL _stp_psm_is_quick_ps_support(VOID);

ENUM_STP_TX_IF_TYPE __weak wmt_plat_get_comm_if_type(VOID)
{
	return STP_MAX_IF_TX;
}

MTK_WCN_BOOL mtk_wcn_stp_psm_dbg_level(INT32 dbglevel)
{
	if (dbglevel >= 0 && dbglevel <= 4) {
		gPsmDbgLevel = dbglevel;
		STP_PSM_PR_INFO("gPsmDbgLevel = %d\n", gPsmDbgLevel);
		return true;
	}
	STP_PSM_PR_INFO("invalid psm debug level. gPsmDbgLevel = %d\n", gPsmDbgLevel);
	return false;
}
#if 0
/* change from macro to static function to enforce type checking on parameters. */
static INT32 psm_fifo_lock_init(MTKSTP_PSM_T *psm)
{


#if CFG_PSM_CORE_FIFO_SPIN_LOCK
#if defined(CONFIG_PROVE_LOCKING)
	osal_unsleepable_lock_init(&(psm->hold_fifo_lock));
	return 0;
#else
	return osal_unsleepable_lock_init(&(psm->hold_fifo_lock));
#endif
#else
#if defined(CONFIG_PROVE_LOCKING)
	osal_sleepable_lock_init(&(psm->hold_fifo_lock));
	return 0;
#else
	return osal_sleepable_lock_init(&(psm->hold_fifo_lock));
#endif
#endif
}

static INT32 psm_fifo_lock_deinit(MTKSTP_PSM_T *psm)
{
#if CFG_PSM_CORE_FIFO_SPIN_LOCK
	return osal_unsleepable_lock_deinit(&(psm->hold_fifo_lock));
#else
	return osal_sleepable_lock_deinit(&(psm->hold_fifo_lock));
#endif
}

static INT32 psm_fifo_lock(MTKSTP_PSM_T *psm)
{


#if CFG_PSM_CORE_FIFO_SPIN_LOCK
	return osal_lock_unsleepable_lock(&(psm->hold_fifo_lock));
#else
	return osal_lock_sleepable_lock(&(psm->hold_fifo_lock));
#endif
}

static INT32 psm_fifo_unlock(MTKSTP_PSM_T *psm)
{


#if CFG_PSM_CORE_FIFO_SPIN_LOCK
	return osal_unlock_unsleepable_lock(&(psm->hold_fifo_lock));
#else
	return osal_unlock_sleepable_lock(&(psm->hold_fifo_lock));
#endif
}
#endif
static INT32 _stp_psm_handler(MTKSTP_PSM_T *stp_psm, P_STP_OP pStpOp)
{
	INT32 ret = -1;

	/* if (NULL == pStpOp) */
	/* { */
	/* return -1; */
	/* } */
	ret = _stp_psm_thread_lock_aquire(stp_psm);
	if (ret) {
		STP_PSM_PR_ERR("--->lock psm_thread_lock failed ret=%d\n", ret);
		return ret;
	}

	switch (pStpOp->opId) {
	case STP_OPID_PSM_EXIT:
		/* TODO: clean all up? */
		ret = 0;
		break;

	case STP_OPID_PSM_SLEEP:
		if (stp_psm_check_sleep_enable(stp_psm) > 0)
			ret = _stp_psm_notify_wmt(stp_psm, SLEEP);
		else
			STP_PSM_PR_INFO("cancel sleep request\n");
		break;

	case STP_OPID_PSM_WAKEUP:
		ret = _stp_psm_notify_wmt(stp_psm, WAKEUP);
		break;

	case STP_OPID_PSM_HOST_AWAKE:
		ret = _stp_psm_notify_wmt(stp_psm, HOST_AWAKE);
		break;

	default:
		STP_PSM_PR_ERR("invalid operation id (%d)\n", pStpOp->opId);
		ret = -1;
		break;
	}
	_stp_psm_thread_lock_release(stp_psm);
	return ret;
}

static P_OSAL_OP _stp_psm_get_op(MTKSTP_PSM_T *stp_psm, P_OSAL_OP_Q pOpQ)
{
	P_OSAL_OP pOp;

	if (!pOpQ) {
		STP_PSM_PR_WARN("pOpQ == NULL\n");
		return NULL;
	}

	osal_lock_unsleepable_lock(&(stp_psm->wq_spinlock));
	/* acquire lock success */
	RB_GET(pOpQ, pOp);

	if ((pOpQ == &stp_psm->rActiveOpQ) && (pOp != NULL)) {
		/* stp_psm->current_active_op = pOp; */
		stp_psm->last_active_opId = pOp->op.opId;
	}
	osal_unlock_unsleepable_lock(&(stp_psm->wq_spinlock));

	if ((pOpQ == &stp_psm->rActiveOpQ) && (pOp != NULL))
		STP_PSM_PR_DBG("last_active_opId(%d)\n", stp_psm->last_active_opId);

	if (!pOp)
		STP_PSM_PR_WARN("RB_GET fail\n");

	return pOp;
}

static INT32 _stp_psm_dump_active_q(P_OSAL_OP_Q pOpQ)
{
	UINT32 read_idx;
	UINT32 write_idx;
	UINT32 opId;

	if (pOpQ == &stp_psm->rActiveOpQ) {
		read_idx = stp_psm->rActiveOpQ.read;
		write_idx = stp_psm->rActiveOpQ.write;

		STP_PSM_PR_DBG("Active op list:++\n");
		while ((read_idx & RB_MASK(pOpQ)) != (write_idx & RB_MASK(pOpQ))) {
			opId = pOpQ->queue[read_idx & RB_MASK(pOpQ)]->op.opId;
			if (opId < STP_OPID_PSM_NUM)
				STP_PSM_PR_DBG("%s\n", g_psm_op_name[opId]);
			else
				STP_PSM_PR_WARN("Unknown OP Id\n");
			++read_idx;
		}
		STP_PSM_PR_DBG("Active op list:--\n");
	} else
		STP_PSM_PR_DBG("%s: not active queue, dont dump\n", __func__);

	return 0;
}

static INT32 _stp_psm_is_redundant_active_op(P_OSAL_OP pOp, P_OSAL_OP_Q pOpQ)
{
	UINT32 opId = 0;
	UINT32 prev_opId = 0;

	/* if((pOpQ == &stp_psm->rActiveOpQ) && (NULL != stp_psm->current_active_op)) */
	if ((pOpQ == &stp_psm->rActiveOpQ) && (stp_psm->last_active_opId != STP_OPID_PSM_INALID)) {
		opId = pOp->op.opId;

		if (opId == STP_OPID_PSM_SLEEP) {
			if (RB_EMPTY(pOpQ)) {
				/* prev_opId = stp_psm->current_active_op->op.opId; */
				prev_opId = stp_psm->last_active_opId;
			} else {
				prev_opId = pOpQ->queue[(pOpQ->write - 1) & RB_MASK(pOpQ)]->op.opId;
			}

			if (prev_opId == STP_OPID_PSM_SLEEP) {
				STP_PSM_PR_DBG("redundant sleep opId found\n");
				return 1;
			} else {
				return 0;
			}
		} else {
			if (RB_EMPTY(pOpQ)) {
				/* prev_opId = stp_psm->current_active_op->op.opId; */
				prev_opId = stp_psm->last_active_opId;
			} else {
				prev_opId = pOpQ->queue[(pOpQ->write - 1) & RB_MASK(pOpQ)]->op.opId;
			}

			if (((opId == STP_OPID_PSM_WAKEUP) && (prev_opId == STP_OPID_PSM_WAKEUP)) ||
			    ((opId == STP_OPID_PSM_HOST_AWAKE)
			     && (prev_opId == STP_OPID_PSM_WAKEUP))
			    || ((opId == STP_OPID_PSM_HOST_AWAKE)
				&& (prev_opId == STP_OPID_PSM_HOST_AWAKE))
			    || ((opId == STP_OPID_PSM_WAKEUP)
				&& (prev_opId == STP_OPID_PSM_HOST_AWAKE))
			    ) {
				STP_PSM_PR_DBG("redundant opId found, opId(%d), preOpid(%d)\n",
						 opId, prev_opId);
				return 1;
			} else {
				return 0;
			}
		}
	} else {
		return 0;
	}

}

static INT32 _stp_psm_clean_up_redundant_active_op(P_OSAL_OP_Q pOpQ)
{
	UINT32 prev_opId = 0;
	UINT32 prev_prev_opId = 0;

	P_OSAL_OP pOp;
	P_OSAL_OP_Q pFreeOpQ = &stp_psm->rFreeOpQ;

	if (pOpQ == &stp_psm->rActiveOpQ) {
		/* sleep , wakeup | sleep, --> null | sleep (x) */
		/* wakeup , sleep , wakeup | sleep --> wakeup | sleep (v) */
		/* sleep , wakeup , sleep | wakeup --> sleep | wakeup (v) */
		/* xxx, sleep | sleep --> xxx, sleep  (v) */
		/* xxx, wakeup | wakeup --> xxx, wakeup  (v) */
		/* xxx, awake | awake --> xxx, awake  (v) --> should never happen */
		while (RB_COUNT(pOpQ) > 2) {
			prev_opId = pOpQ->queue[(pOpQ->write - 1) & RB_MASK(pOpQ)]->op.opId;
			prev_prev_opId = pOpQ->queue[(pOpQ->write - 2) & RB_MASK(pOpQ)]->op.opId;

			if ((prev_opId == STP_OPID_PSM_SLEEP
			     && prev_prev_opId == STP_OPID_PSM_WAKEUP)
			    || (prev_opId == STP_OPID_PSM_SLEEP
				&& prev_prev_opId == STP_OPID_PSM_HOST_AWAKE)
			    || (prev_opId == STP_OPID_PSM_WAKEUP
				&& prev_prev_opId == STP_OPID_PSM_SLEEP)
			    || (prev_opId == STP_OPID_PSM_HOST_AWAKE
				&& prev_prev_opId == STP_OPID_PSM_SLEEP)
			    ) {
				RB_GET(pOpQ, pOp);
				RB_PUT(pFreeOpQ, pOp);
				RB_GET(pOpQ, pOp);
				RB_PUT(pFreeOpQ, pOp);
			} else if (prev_opId == prev_prev_opId) {
				RB_GET(pOpQ, pOp);
				if (!pOp) {
					STP_PSM_PR_DBG("RB_GET pOp == NULL\n");
				} else {
					STP_PSM_PR_DBG("redundant opId(%d) found, remove it\n",
						 pOp->op.opId);
				}
				RB_PUT(pFreeOpQ, pOp);
			} else
			    if ((prev_opId == STP_OPID_PSM_WAKEUP
				 && prev_prev_opId == STP_OPID_PSM_HOST_AWAKE)
				|| (prev_opId == STP_OPID_PSM_HOST_AWAKE
				    && prev_prev_opId == STP_OPID_PSM_WAKEUP)) {
				STP_PSM_PR_WARN("prev_opId(%d), prev_prev_opId(%d)\n", prev_opId,
						  prev_prev_opId);
				RB_GET(pOpQ, pOp);
				RB_PUT(pFreeOpQ, pOp);
			} else {
				STP_PSM_PR_ERR
				    ("prev_opId(%d), prev_prev_opId(%d), this should never happen!!!\n",
				     prev_opId, prev_prev_opId);
				break;
			}
		}
	}

	return 0;
}

static INT32 _stp_psm_put_op(MTKSTP_PSM_T *stp_psm, P_OSAL_OP_Q pOpQ, P_OSAL_OP pOp)
{
	INT32 ret;

	/* if (!pOpQ || !pOp) */
	/* { */
	/* STP_PSM_PR_WARN("pOpQ = 0x%p, pLxOp = 0x%p\n", pOpQ, pOp); */
	/* return 0; */
	/* } */
	ret = 0;

	osal_lock_unsleepable_lock(&(stp_psm->wq_spinlock));
	/* acquire lock success */
	if (pOpQ == &stp_psm->rActiveOpQ) {
		if (!_stp_psm_is_redundant_active_op(pOp, pOpQ)) {
			/* acquire lock success */
			if (!RB_FULL(pOpQ)) {
				RB_PUT(pOpQ, pOp);
				STP_PSM_PR_DBG("opId(%d) enqueue\n", pOp->op.opId);
			} else {
				STP_PSM_PR_INFO("************ Active Queue Full ************\n");
				ret = -1;
			}

			_stp_psm_clean_up_redundant_active_op(pOpQ);
		} else {
			/*redundant opId, mark ret as success */
			P_OSAL_OP_Q pFreeOpQ = &stp_psm->rFreeOpQ;

			if (!RB_FULL(pFreeOpQ))
				RB_PUT(pFreeOpQ, pOp);
			else
				osal_assert(!RB_FULL(pFreeOpQ));
			ret = 0;
		}
	} else {
		if (!RB_FULL(pOpQ))
			RB_PUT(pOpQ, pOp);
		else
			ret = -1;
	}

	if (pOpQ == &stp_psm->rActiveOpQ)
		_stp_psm_dump_active_q(&stp_psm->rActiveOpQ);


	if (ret) {
		STP_PSM_PR_WARN("RB_FULL, RB_COUNT=%d , RB_SIZE=%d\n", RB_COUNT(pOpQ),
				  RB_SIZE(pOpQ));
		osal_opq_dump_locked("FreeOpQ", &stp_psm->rFreeOpQ);
		osal_opq_dump_locked("ActiveOpQ", &stp_psm->rActiveOpQ);
	}

	osal_unlock_unsleepable_lock(&(stp_psm->wq_spinlock));
	return ret ? 0 : 1;
}

P_OSAL_OP _stp_psm_get_free_op(MTKSTP_PSM_T *stp_psm)
{
	P_OSAL_OP pOp;

	if (stp_psm) {
		pOp = _stp_psm_get_op(stp_psm, &stp_psm->rFreeOpQ);
		if (pOp) {
			osal_memset(pOp, 0, osal_sizeof(OSAL_OP));
		}
		return pOp;
	}
	return NULL;
}

INT32 _stp_psm_put_act_op(MTKSTP_PSM_T *stp_psm, P_OSAL_OP pOp)
{
	INT32 bRet = 0;		/* MTK_WCN_BOOL_FALSE; */
	INT32 wait_ret = -1;
	P_OSAL_SIGNAL pSignal = NULL;
	INT32 ret = 0;

	do {
		if (!stp_psm || !pOp) {
			STP_PSM_PR_ERR("stp_psm = %p, pOp = %p\n", stp_psm, pOp);
			break;
		}

		pSignal = &pOp->signal;

		if (pSignal->timeoutValue) {
			pOp->result = -9;
			osal_signal_init(&pOp->signal);
		}

		/* Init ref_count to 2, as one is held by current thread, the second by psm thread */
		atomic_set(&pOp->ref_count, 2);

		/* put to active Q */
		bRet = _stp_psm_put_op(stp_psm, &stp_psm->rActiveOpQ, pOp);

		if (bRet == 0) {
			STP_PSM_PR_WARN("+++++++++++ Put op Active queue Fail\n");
			atomic_dec(&pOp->ref_count);
			break;
		}
		_stp_psm_opid_dbg_dmp_in(g_stp_psm_opid_dbg, pOp->op.opId, __LINE__);
		/* wake up wmtd */
		ret = osal_trigger_event(&stp_psm->STPd_event);

		if (pSignal->timeoutValue == 0) {
			bRet = 1;	/* MTK_WCN_BOOL_TRUE; */
			break;
		}

		/* check result */
		wait_ret = osal_wait_for_signal_timeout(&pOp->signal, &stp_psm->PSMd);
		STP_PSM_PR_DBG("wait completion:%d\n", wait_ret);
		if (!wait_ret) {
			STP_PSM_PR_ERR("wait completion timeout\n");
			/* TODO: how to handle it? retry? */
		} else {
			if (pOp->result)
				STP_PSM_PR_WARN("op(%d) result:%d\n", pOp->op.opId, pOp->result);
			/* op completes, check result */
			bRet = (pOp->result) ? 0 : 1;
		}
	} while (0);

	if (pOp && atomic_dec_and_test(&pOp->ref_count)) {
		/* put Op back to freeQ */
		bRet = _stp_psm_put_op(stp_psm, &stp_psm->rFreeOpQ, pOp);
		if (bRet == 0)
			STP_PSM_PR_WARN("+++++++++++ Put op active free fail, maybe disable/enable psm\n");
	}

	return bRet;
}

static INT32 _stp_psm_wait_for_msg(PVOID pvData)
{
	MTKSTP_PSM_T *stp_psm = (MTKSTP_PSM_T *)pvData;

	STP_PSM_PR_DBG("%s: stp_psm->rActiveOpQ = %d\n", __func__,
			 RB_COUNT(&stp_psm->rActiveOpQ));

	return (!RB_EMPTY(&stp_psm->rActiveOpQ)) || osal_thread_should_stop(&stp_psm->PSMd);
}

static INT32 _stp_psm_proc(PVOID pvData)
{
	MTKSTP_PSM_T *stp_psm = (MTKSTP_PSM_T *) pvData;
	P_OSAL_OP pOp;
	UINT32 id;
	INT32 result;

	if (!stp_psm) {
		STP_PSM_PR_WARN("!stp_psm\n");
		return -1;
	}
/* STP_PSM_PR_INFO("wmtd starts running: pWmtDev(0x%p) [pol, rt_pri, n_pri, pri]=[%d, %d, %d, %d]\n", */
/* stp_psm, current->policy, current->rt_priority, current->normal_prio, current->prio); */

	for (;;) {

		pOp = NULL;
		osal_wait_for_event(&stp_psm->STPd_event, _stp_psm_wait_for_msg, (PVOID) stp_psm);

		/* we set reset flag when calling stp_reset after cleanup all op. */
		if (osal_test_bit(STP_PSM_RESET_EN, &stp_psm->flag)) {
			osal_clear_bit(STP_PSM_RESET_EN, &stp_psm->flag);
			_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
		}
		if (osal_thread_should_stop(&stp_psm->PSMd)) {
			STP_PSM_PR_INFO("should stop now...\n");
			/* TODO: clean up active opQ */
			break;
		}

		/* get Op from activeQ */
		pOp = _stp_psm_get_op(stp_psm, &stp_psm->rActiveOpQ);
		if (!pOp) {
			STP_PSM_PR_WARN
			    ("+++++++++++ Get op from activeQ fail, maybe disable/enable psm\n");
			continue;
		}
		osal_op_history_save(&stp_psm->op_history, pOp);

		id = osal_op_get_id(pOp);

		if (id >= STP_OPID_PSM_NUM) {
			STP_PSM_PR_WARN("abnormal opid id: 0x%x\n", id);
			result = -1;
			goto handler_done;
		}

		result = _stp_psm_handler(stp_psm, &pOp->op);


handler_done:

		if (result)
			STP_PSM_PR_WARN("opid id(0x%x)(%s) error(%d)\n", id,
					(id >= 4) ? ("???") : (g_psm_op_name[id]), result);

		if (atomic_dec_and_test(&pOp->ref_count)) {
			/* put Op back to freeQ */
			if (_stp_psm_put_op(stp_psm, &stp_psm->rFreeOpQ, pOp) == 0)
				STP_PSM_PR_WARN
					("+++++++++++ Put op to FreeOpQ fail, maybe disable/enable psm\n");
		} else if (osal_op_is_wait_for_signal(pOp)) {
			osal_op_raise_signal(pOp, result);
		}

		if (id == STP_OPID_PSM_EXIT)
			break;
	}
	STP_PSM_PR_INFO("exits\n");

	return 0;
};

static inline INT32 _stp_psm_get_time(VOID)
{
	if (gPsmDbgLevel >= STP_PSM_LOG_LOUD)
		osal_printtimeofday("<psm time>>>>");

	return 0;
}

static inline INT32 _stp_psm_get_state(MTKSTP_PSM_T *stp_psm)
{
	if (stp_psm == NULL)
		return STP_PSM_OPERATION_FAIL;
	if (stp_psm->work_state < STP_PSM_MAX_STATE)
		return stp_psm->work_state;
	STP_PSM_PR_ERR("work_state = %d, invalid\n", stp_psm->work_state);
	return -STP_PSM_OPERATION_FAIL;
}

static inline INT32 _stp_psm_set_state(MTKSTP_PSM_T *stp_psm, const MTKSTP_PSM_STATE_T state)
{
	if (stp_psm == NULL)
		return STP_PSM_OPERATION_FAIL;
	if (stp_psm->work_state < STP_PSM_MAX_STATE) {
		_stp_psm_get_time();
		/* STP_PSM_PR_INFO("work_state = %s --> %s\n",
		 * g_psm_state[stp_psm->work_state], g_psm_state[state]);
		 */
		stp_psm->work_state = state;
		if (stp_psm->work_state != ACT) {
			/* osal_lock_unsleepable_lock(&stp_psm->flagSpinlock); */
			osal_set_bit(STP_PSM_BLOCK_DATA_EN, &stp_psm->flag);
			/* osal_unlock_unsleepable_lock(&stp_psm->flagSpinlock); */
		}
	} else
		STP_PSM_PR_ERR("work_state = %d, invalid\n", stp_psm->work_state);

	return STP_PSM_OPERATION_SUCCESS;
}

static inline INT32 _stp_psm_start_monitor(MTKSTP_PSM_T *stp_psm)
{

	if (!stp_psm)
		return STP_PSM_OPERATION_FAIL;

	if (osal_test_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR, &stp_psm->flag)) {
		STP_PSM_PR_DBG("STP-PSM DISABLE, DONT restart monitor!\n\r");
		return STP_PSM_OPERATION_SUCCESS;
	}


	STP_PSM_PR_LOUD("start monitor\n");
	stp_traffic_start = stp_traffic_current;
	osal_timer_modify(&stp_psm->psm_timer, stp_psm->idle_time_to_sleep);

	return STP_PSM_OPERATION_SUCCESS;
}

static inline INT32 _stp_psm_stop_monitor(MTKSTP_PSM_T *stp_psm)
{
	if (!stp_psm)
		return STP_PSM_OPERATION_FAIL;
	STP_PSM_PR_DBG("stop monitor\n");
	osal_timer_stop_sync(&stp_psm->psm_timer);
	return STP_PSM_OPERATION_SUCCESS;
}

INT32
_stp_psm_hold_data(MTKSTP_PSM_T *stp_psm, const PUINT8 buffer, const UINT32 len, const UINT8 type)
{
	INT32 available_space = 0;
	INT32 needed_space = 0;
	UINT8 delimiter[] = { 0xbb, 0xbb };

	if (!stp_psm)
		return STP_PSM_OPERATION_FAIL;
	/*psm_fifo_lock(stp_psm);*/
	osal_lock_sleepable_lock(&stp_psm->hold_fifo_spinlock_global);
	available_space = STP_PSM_FIFO_SIZE - osal_fifo_len(&stp_psm->hold_fifo);
	needed_space = len + sizeof(UINT8) + sizeof(UINT32) + 2;
	/* STP_PSM_PR_INFO("*******FIFO Available(%d), Need(%d)\n",
	 * available_space, needed_space);
	 */
	if (available_space < needed_space) {
		STP_PSM_PR_ERR("FIFO Available!! Reset FIFO\n");
		osal_fifo_reset(&stp_psm->hold_fifo);
	}
	/* type */
	osal_fifo_in(&stp_psm->hold_fifo, (PUINT8) &type, sizeof(UINT8));
	/* length */
	osal_fifo_in(&stp_psm->hold_fifo, (PUINT8) &len, sizeof(UINT32));
	/* buffer */
	osal_fifo_in(&stp_psm->hold_fifo, (PUINT8) buffer, len);
	/* delimiter */
	osal_fifo_in(&stp_psm->hold_fifo, (PUINT8) delimiter, 2);
	/*psm_fifo_unlock(stp_psm);*/
	osal_unlock_sleepable_lock(&stp_psm->hold_fifo_spinlock_global);
	return len;
}

INT32 _stp_psm_has_pending_data(MTKSTP_PSM_T *stp_psm)
{
	return osal_fifo_len(&stp_psm->hold_fifo);
}

INT32 _stp_psm_release_data(MTKSTP_PSM_T *stp_psm)
{

	INT32 i = 20;		/*Max buffered packet number */
	INT32 ret = 0;
	UINT8 type = 0;
	UINT32 len = 0;
	UINT8 delimiter[2] = {0};
	INT32 winspace_flag = 0;

	/* STP_PSM_PR_ERR("++++++++++release data++len=%d\n", osal_fifo_len(&stp_psm->hold_fifo)); */
	while ((osal_fifo_len(&stp_psm->hold_fifo) && i > 0) || winspace_flag > 0) {
		/* acquire spinlock */
		/* psm_fifo_lock(stp_psm); */
		osal_lock_sleepable_lock(&stp_psm->hold_fifo_spinlock_global);

		if (winspace_flag == 0) {
			ret = osal_fifo_out(&stp_psm->hold_fifo, (PUINT8)&type, sizeof(UINT8));
			ret = osal_fifo_out(&stp_psm->hold_fifo, (PUINT8)&len, sizeof(UINT32));

			if (len > STP_PSM_PACKET_SIZE_MAX) {
				STP_PSM_PR_ERR("***psm packet's length too Long!****\n");
				STP_PSM_PR_INFO("***reset psm's fifo***\n");
			} else {
				osal_memset(stp_psm->out_buf, 0, STP_PSM_TX_SIZE);
				ret = osal_fifo_out(&stp_psm->hold_fifo, (PUINT8) stp_psm->out_buf, len);
			}

			ret = osal_fifo_out(&stp_psm->hold_fifo, (PUINT8)delimiter, 2);
		}

		if (delimiter[0] == 0xbb && delimiter[1] == 0xbb) {
			/* osal_buffer_dump(stp_psm->out_buf, "psm->out_buf", len, 32); */
			ret = stp_send_data_no_ps(stp_psm->out_buf, len, type);
			if (ret == 0)
				winspace_flag++;
			else
				winspace_flag = 0;
		} else {
			STP_PSM_PR_ERR("***psm packet fifo parsing fail****\n");
			STP_PSM_PR_INFO("***reset psm's fifo***\n");

			osal_fifo_reset(&stp_psm->hold_fifo);
		}

		if (winspace_flag == 0)
			i--;
		/* psm_fifo_unlock(stp_psm); */
		osal_unlock_sleepable_lock(&stp_psm->hold_fifo_spinlock_global);

		if (winspace_flag > 0 && winspace_flag < 10)
			osal_sleep_ms(2);
		else if (winspace_flag >= 10) {
			STP_PSM_PR_ERR("***More than 20ms no winspace available***\n");
			break;
		}
	}
	return STP_PSM_OPERATION_SUCCESS;
}

static inline INT32 _stp_psm_notify_wmt_host_awake_wq(MTKSTP_PSM_T *stp_psm)
{

	P_OSAL_OP pOp;
	INT32 bRet;
	INT32 retval;

	if (stp_psm == NULL)
		return STP_PSM_OPERATION_FAIL;
	pOp = _stp_psm_get_free_op(stp_psm);
	if (!pOp) {
		STP_PSM_PR_DBG("get_free_lxop fail\n");
		return -1;	/* break; */
	}
	pOp->op.opId = STP_OPID_PSM_HOST_AWAKE;
	pOp->signal.timeoutValue = 0;
	bRet = _stp_psm_put_act_op(stp_psm, pOp);
	STP_PSM_PR_DBG("OPID(%d) type(%zd) bRet(%d)\n\n", pOp->op.opId, pOp->op.au4OpData[0], bRet);
	retval = (bRet == 0) ? (STP_PSM_OPERATION_FAIL) : 0;
	return retval;
}

static inline INT32 _stp_psm_notify_wmt_wakeup_wq(MTKSTP_PSM_T *stp_psm)
{
	P_OSAL_OP pOp;
	INT32 bRet;
	INT32 retval;

	if (stp_psm == NULL)
		return STP_PSM_OPERATION_FAIL;
	pOp = _stp_psm_get_free_op(stp_psm);
	if (!pOp) {
		STP_PSM_PR_DBG("get_free_lxop fail\n");
		return -1;	/* break; */
	}
	pOp->op.opId = STP_OPID_PSM_WAKEUP;
	pOp->signal.timeoutValue = 0;
	bRet = _stp_psm_put_act_op(stp_psm, pOp);
	if (bRet == 0) {
		STP_PSM_PR_WARN("OPID(%d) type(%zd) bRet(%s)\n\n",
				pOp->op.opId, pOp->op.au4OpData[0], "fail");
	}
	retval = (bRet == 0) ? (STP_PSM_OPERATION_FAIL) : (STP_PSM_OPERATION_SUCCESS);
	return retval;
}

static inline INT32 _stp_psm_notify_wmt_sleep_wq(MTKSTP_PSM_T *stp_psm)
{
	P_OSAL_OP pOp;
	INT32 bRet;
	INT32 retval;

	if (stp_psm == NULL)
		return STP_PSM_OPERATION_FAIL;
	if (osal_test_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR_TX_HIGH_DENSITY, &stp_psm->flag))
		return 0;
#if PSM_USE_COUNT_PACKAGE
	if (osal_test_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR_RX_HIGH_DENSITY, &stp_psm->flag))
		return 0;
#endif
	if (osal_test_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR, &stp_psm->flag))
		return 0;
	pOp = _stp_psm_get_free_op(stp_psm);
	if (!pOp) {
		STP_PSM_PR_DBG("get_free_lxop fail\n");
		return -1;	/* break; */
	}
	pOp->op.opId = STP_OPID_PSM_SLEEP;
	pOp->signal.timeoutValue = 0;
	bRet = _stp_psm_put_act_op(stp_psm, pOp);
	STP_PSM_PR_DBG("OPID(%d) type(%zd) bRet(%d)\n\n", pOp->op.opId, pOp->op.au4OpData[0], bRet);
	retval = (bRet == 0) ? (STP_PSM_OPERATION_FAIL) : 1;
	return retval;
}

/*internal function*/

static inline INT32 _stp_psm_reset(MTKSTP_PSM_T *stp_psm)
{
	INT32 i = 0;
	P_OSAL_OP_Q pOpQ;
	P_OSAL_OP pOp;
	INT32 ret = 0;

	STP_PSM_PR_DBG("PSM MODE RESET=============================>\n\r");
	STP_PSM_PR_DBG("_stp_psm_reset\n");
	STP_PSM_PR_DBG("reset-wake_lock(%d)\n", osal_wake_lock_count(&stp_psm->wake_lock));
	osal_wake_unlock(&stp_psm->wake_lock);
	STP_PSM_PR_DBG("reset-wake_lock(%d)\n", osal_wake_lock_count(&stp_psm->wake_lock));
	/* --> serialized the request from wmt <--// */
	ret = osal_lock_sleepable_lock(&stp_psm->user_lock);
	if (ret) {
		STP_PSM_PR_ERR("--->lock stp_psm->user_lock failed, ret=%d\n", ret);
		return ret;
	}
	/* --> disable psm <--// */
	stp_psm->flag.data = 0;
	osal_set_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR, &stp_psm->flag);
	_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
	_stp_psm_stop_monitor(stp_psm);
	/* --> prepare the op list <--// */
	osal_lock_unsleepable_lock(&(stp_psm->wq_spinlock));
	RB_INIT(&stp_psm->rFreeOpQ, STP_OP_BUF_SIZE);
	RB_INIT(&stp_psm->rActiveOpQ, STP_OP_BUF_SIZE);
	/* stp_psm->current_active_op = NULL; */
	stp_psm->last_active_opId = STP_OPID_PSM_INALID;
	pOpQ = &stp_psm->rFreeOpQ;
	for (i = 0; i < STP_OP_BUF_SIZE; i++) {
		if (!RB_FULL(pOpQ)) {
			pOp = &stp_psm->arQue[i];
			RB_PUT(pOpQ, pOp);
		}
	}
	osal_unlock_unsleepable_lock(&(stp_psm->wq_spinlock));
	/* --> clean up interal data structure<--// */
	_stp_psm_set_state(stp_psm, ACT);
	/*psm_fifo_lock(stp_psm);*/
	osal_lock_sleepable_lock(&stp_psm->hold_fifo_spinlock_global);
	osal_fifo_reset(&stp_psm->hold_fifo);
	/*psm_fifo_unlock(stp_psm);*/
	osal_unlock_sleepable_lock(&stp_psm->hold_fifo_spinlock_global);
	/* --> stop psm thread wait <--*/
	osal_set_bit(STP_PSM_RESET_EN, &stp_psm->flag);
	_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
	osal_trigger_event(&stp_psm->wait_wmt_q);
	osal_unlock_sleepable_lock(&stp_psm->user_lock);
	STP_PSM_PR_DBG("PSM MODE RESET<============================\n\r");
	return STP_PSM_OPERATION_SUCCESS;
}

static INT32 _stp_psm_wait_wmt_event(PVOID pvData)
{
	MTKSTP_PSM_T *stp_psm = (MTKSTP_PSM_T *) pvData;

	STP_PSM_PR_DBG("%s, stp_psm->flag= %ld\n", __func__, stp_psm->flag.data);
	osal_ftrace_print("%s, stp_psm->flag= %ld\n", __func__, stp_psm->flag.data);

	return (osal_test_bit(STP_PSM_WMT_EVENT_SLEEP_EN, &stp_psm->flag)) ||
		(osal_test_bit(STP_PSM_WMT_EVENT_WAKEUP_EN, &stp_psm->flag)) ||
		(osal_test_bit(STP_PSM_WMT_EVENT_ROLL_BACK_EN, &stp_psm->flag)) ||
		(osal_test_bit(STP_PSM_RESET_EN, &stp_psm->flag));

}


static inline INT32 _stp_psm_wait_wmt_event_wq(MTKSTP_PSM_T *stp_psm)
{

	INT32 retval = 0;
	PUINT8 pbuf = NULL;
	INT32 len = 0;

	if (stp_psm == NULL)
		return STP_PSM_OPERATION_FAIL;
	osal_wait_for_event_timeout(&stp_psm->wait_wmt_q, _stp_psm_wait_wmt_event, (PVOID) stp_psm);

	if (mtk_wcn_stp_get_wmt_trg_assert() == 1 || mtk_wcn_stp_coredump_start_get() == 1) {
		STP_PSM_PR_INFO("Host/Fw already triggered assert. Skip psm operation.\n");
		return STP_PSM_OPERATION_FAIL;
	}

	if (osal_test_bit(STP_PSM_WMT_EVENT_WAKEUP_EN, &stp_psm->flag)) {
		osal_clear_bit(STP_PSM_WMT_EVENT_WAKEUP_EN, &stp_psm->flag);
		_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
		/* osal_lock_unsleepable_lock(&stp_psm->flagSpinlock); */
		/* STP send data here: STP enqueue data to psm buffer. */
		_stp_psm_release_data(stp_psm);
		/* STP send data here: STP enqueue data to psm buffer. We release packet by the next one. */
		osal_clear_bit(STP_PSM_BLOCK_DATA_EN, &stp_psm->flag);
		_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
		/* STP send data here: STP sends data directly without PSM. */
		_stp_psm_set_state(stp_psm, ACT);
		/* osal_unlock_unsleepable_lock(&stp_psm->flagSpinlock); */
		if (stp_psm_is_quick_ps_support())
			stp_psm_notify_wmt_sleep(stp_psm);
		else
			_stp_psm_start_monitor(stp_psm);
	} else if (osal_test_bit(STP_PSM_WMT_EVENT_SLEEP_EN, &stp_psm->flag)) {
		osal_clear_bit(STP_PSM_WMT_EVENT_SLEEP_EN, &stp_psm->flag);
		_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
		_stp_psm_set_state(stp_psm, INACT);
		STP_PSM_PR_DBG("mt_combo_plt_enter_deep_idle++\n");
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
			mt_combo_plt_enter_deep_idle(COMBO_IF_BTIF);
		else {
			switch (wmt_plat_get_comm_if_type()) {
			case STP_UART_IF_TX:
				mt_combo_plt_enter_deep_idle(COMBO_IF_UART);
				break;
			case STP_SDIO_IF_TX:
				mt_combo_plt_enter_deep_idle(COMBO_IF_MSDC);
				break;
			default:
				break;
			}
		}
		STP_PSM_PR_DBG("mt_combo_plt_enter_deep_idle--\n");
		STP_PSM_PR_DBG("sleep-wake_lock(%d)\n", osal_wake_lock_count(&stp_psm->wake_lock));
		osal_wake_unlock(&stp_psm->wake_lock);
		STP_PSM_PR_DBG("sleep-wake_lock#(%d)\n", osal_wake_lock_count(&stp_psm->wake_lock));

		if (osal_wake_lock_count(&stp_psm->wake_lock) == 0 && stp_psm->update_wmt_fw_patch_chip_rst != NULL)
			stp_psm->update_wmt_fw_patch_chip_rst();
	} else if (osal_test_bit(STP_PSM_WMT_EVENT_ROLL_BACK_EN, &stp_psm->flag)) {
		osal_clear_bit(STP_PSM_WMT_EVENT_ROLL_BACK_EN, &stp_psm->flag);
		_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
		if (_stp_psm_get_state(stp_psm) == ACT_INACT) {
			/* osal_lock_unsleepable_lock(&stp_psm->flagSpinlock); */
			_stp_psm_release_data(stp_psm);
			osal_clear_bit(STP_PSM_BLOCK_DATA_EN, &stp_psm->flag);
			_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			_stp_psm_set_state(stp_psm, ACT);
			/* osal_unlock_unsleepable_lock(&stp_psm->flagSpinlock); */
		} else if (_stp_psm_get_state(stp_psm) == INACT_ACT) {
			_stp_psm_set_state(stp_psm, INACT);
			STP_PSM_PR_INFO("[WARNING]PSM state rollback due too wakeup fail\n");
		}
	} else if (osal_test_bit(STP_PSM_RESET_EN, &stp_psm->flag)) {
		osal_clear_bit(STP_PSM_WMT_EVENT_ROLL_BACK_EN, &stp_psm->flag);
		_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
	} else {
		STP_PSM_PR_ERR("state = %d, flag = %ld<== Abnormal flag be set!!\n\r",
				stp_psm->work_state, stp_psm->flag.data);
		mtk_wcn_wmt_dump_wmtd_backtrace();
		/* wcn_psm_flag_trigger_collect_ftrace(); */	/* trigger collect SYS_FTRACE */
		pbuf = "Abnormal PSM flag be set, just collect SYS_FTRACE to DB";
		len = osal_strlen(pbuf);
		stp_dbg_trigger_collect_ftrace(pbuf, len);
		_stp_psm_dbg_out_printk(g_stp_psm_dbg);
	}
	retval = STP_PSM_OPERATION_SUCCESS;
	return retval;
}

static inline INT32 _stp_psm_notify_stp(MTKSTP_PSM_T *stp_psm, const MTKSTP_PSM_ACTION_T action)
{

	INT32 retval = STP_PSM_OPERATION_SUCCESS;

	if (action < 0 || action >= STP_PSM_MAX_ACTION)
		return STP_PSM_OPERATION_FAIL;

	if (action == EIRQ) {
		STP_PSM_PR_DBG("Call _stp_psm_notify_wmt_host_awake_wq\n\r");
		_stp_psm_notify_wmt_host_awake_wq(stp_psm);
		return STP_PSM_OPERATION_FAIL;
	}
	if ((_stp_psm_get_state(stp_psm) < STP_PSM_MAX_STATE) && (_stp_psm_get_state(stp_psm) >= 0)) {
		STP_PSM_PR_DBG("state = %s, action=%s\n\r",
				 g_psm_state[_stp_psm_get_state(stp_psm)], g_psm_action[action]);
	}
	/* If STP trigger WAKEUP and SLEEP, to do the job below */
	switch (_stp_psm_get_state(stp_psm)) {
		/* stp trigger */
	case ACT_INACT:
		if (action == SLEEP) {
			STP_PSM_PR_LOUD("Action = %s, ACT_INACT state, ready to INACT\n\r",
					  g_psm_action[action]);
			osal_clear_bit(STP_PSM_WMT_EVENT_WAKEUP_EN, &stp_psm->flag);
			_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			osal_set_bit(STP_PSM_WMT_EVENT_SLEEP_EN, &stp_psm->flag);
			_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			/* wake_up(&stp_psm->wait_wmt_q); */
			osal_trigger_event(&stp_psm->wait_wmt_q);
		} else if (action == ROLL_BACK) {
			STP_PSM_PR_LOUD("Action = %s, ACT_INACT state, back to ACT\n\r",
					  g_psm_action[action]);
			/* stp_psm->flag &= ~STP_PSM_WMT_EVENT_ROLL_BACK_EN; */
			osal_set_bit(STP_PSM_WMT_EVENT_ROLL_BACK_EN, &stp_psm->flag);
			_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			/* wake_up(&stp_psm->wait_wmt_q); */
			osal_trigger_event(&stp_psm->wait_wmt_q);
		} else {
			if (action < STP_PSM_MAX_ACTION) {
				STP_PSM_PR_ERR("Action = %s, state = %d, flag = %ld, abnormal!\n",
						g_psm_action[action],
						stp_psm->work_state, stp_psm->flag.data);
			} else {
				STP_PSM_PR_ERR("Invalid Action!!\n\r");
			}
			_stp_psm_dbg_out_printk(g_stp_psm_dbg);

			retval = STP_PSM_OPERATION_FAIL;
		}
		break;
		/* stp trigger */
	case INACT_ACT:
		if (action == WAKEUP) {
			STP_PSM_PR_LOUD("Action = %s, INACT_ACT state, ready to ACT\n\r",
					  g_psm_action[action]);
			osal_clear_bit(STP_PSM_WMT_EVENT_SLEEP_EN, &stp_psm->flag);
			_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			osal_set_bit(STP_PSM_WMT_EVENT_WAKEUP_EN, &stp_psm->flag);
			_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			/* wake_up(&stp_psm->wait_wmt_q); */
			osal_trigger_event(&stp_psm->wait_wmt_q);
		} else if (action == HOST_AWAKE) {
			STP_PSM_PR_LOUD("Action = %s, INACT_ACT state, ready to ACT\n\r",
					  g_psm_action[action]);
			osal_clear_bit(STP_PSM_WMT_EVENT_SLEEP_EN, &stp_psm->flag);
			_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			osal_set_bit(STP_PSM_WMT_EVENT_WAKEUP_EN, &stp_psm->flag);
			_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			/* wake_up(&stp_psm->wait_wmt_q); */
			osal_trigger_event(&stp_psm->wait_wmt_q);
		} else if (action == ROLL_BACK) {
			STP_PSM_PR_LOUD("Action = %s, INACT_ACT state, back to INACT\n\r",
					  g_psm_action[action]);
			/* stp_psm->flag &= ~STP_PSM_WMT_EVENT_ROLL_BACK_EN; */
			osal_set_bit(STP_PSM_WMT_EVENT_ROLL_BACK_EN, &stp_psm->flag);
			_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			/* wake_up(&stp_psm->wait_wmt_q); */
			osal_trigger_event(&stp_psm->wait_wmt_q);
		} else {
			if (action < STP_PSM_MAX_ACTION) {
				STP_PSM_PR_ERR("Action = %s, state = %d, flag = %ld, abnormal!\n",
						g_psm_action[action],
						stp_psm->work_state, stp_psm->flag.data);
			} else {
				STP_PSM_PR_ERR("Invalid Action!!\n\r");
			}
			_stp_psm_dbg_out_printk(g_stp_psm_dbg);
			retval = STP_PSM_OPERATION_FAIL;
		}
		break;
	case INACT:
		if (action < STP_PSM_MAX_ACTION) {
			STP_PSM_PR_ERR("Action = %s, state = %d, flag = %ld, abnormal!\n",
					g_psm_action[action],
					stp_psm->work_state, stp_psm->flag.data);
		} else {
			STP_PSM_PR_ERR("Invalid Action!!\n\r");
		}
		_stp_psm_dbg_out_printk(g_stp_psm_dbg);
		retval = -1;
		break;
	case ACT:
		if (action < STP_PSM_MAX_ACTION) {
			STP_PSM_PR_ERR("Action = %s, state = %d, flag = %ld, abnormal!\n",
					g_psm_action[action],
					stp_psm->work_state, stp_psm->flag.data);
		} else {
			STP_PSM_PR_ERR("Invalid Action!!\n\r");
		}
		_stp_psm_dbg_out_printk(g_stp_psm_dbg);

		retval = STP_PSM_OPERATION_FAIL;
		break;
	default:
		/*invalid */
		if (action < STP_PSM_MAX_ACTION) {
			STP_PSM_PR_ERR("Action = %s, state = %d, flag = %ld, abnormal!\n",
					g_psm_action[action],
					stp_psm->work_state, stp_psm->flag.data);
		} else {
			STP_PSM_PR_ERR("Invalid Action!!\n\r");
		}
		_stp_psm_dbg_out_printk(g_stp_psm_dbg);

		retval = STP_PSM_OPERATION_FAIL;
		break;
	}
	return retval;
}

static inline INT32 _stp_psm_notify_wmt(MTKSTP_PSM_T *stp_psm, const MTKSTP_PSM_ACTION_T action)
{
	INT32 ret = STP_PSM_OPERATION_SUCCESS;

	if (stp_psm == NULL || action < 0 || action >= STP_PSM_MAX_ACTION)
		return STP_PSM_OPERATION_FAIL;

	switch (_stp_psm_get_state(stp_psm)) {
	case ACT:

		if (action == SLEEP) {
			osal_lock_sleepable_lock(&stp_psm->user_lock);
			if (osal_test_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR, &stp_psm->flag)) {
				STP_PSM_PR_ERR("psm monitor disabled, can't do sleep op\n");
				osal_unlock_sleepable_lock(&stp_psm->user_lock);
				return STP_PSM_OPERATION_FAIL;
			}

			_stp_psm_set_state(stp_psm, ACT_INACT);
			osal_unlock_sleepable_lock(&stp_psm->user_lock);
			_stp_psm_release_data(stp_psm);

			if (stp_psm->wmt_notify) {
				ret = stp_psm->wmt_notify(SLEEP);
				if (!ret)
					_stp_psm_wait_wmt_event_wq(stp_psm);
				else
					STP_PSM_PR_ERR("stp_psm->wmt_notify return fail\n");

			} else {
				STP_PSM_PR_ERR("stp_psm->wmt_notify = NULL\n");
				ret = STP_PSM_OPERATION_FAIL;
			}
		} else if (action == WAKEUP || action == HOST_AWAKE) {
			STP_PSM_PR_DBG("In ACT state, dont do WAKEUP/HOST_AWAKE again\n");
			_stp_psm_release_data(stp_psm);
		} else {
			STP_PSM_PR_ERR("Action = %s, state = %d, flag = %ld, abnormal!\n",
					g_psm_action[action],
					stp_psm->work_state, stp_psm->flag.data);
			_stp_psm_dbg_out_printk(g_stp_psm_dbg);
			ret = STP_PSM_OPERATION_FAIL;

		}

		break;

	case INACT:

		if (action == WAKEUP) {
			_stp_psm_set_state(stp_psm, INACT_ACT);

			if (stp_psm->wmt_notify) {
				STP_PSM_PR_DBG("wakeup +wake_lock(%d)\n",
						 osal_wake_lock_count(&stp_psm->wake_lock));
				osal_wake_lock(&stp_psm->wake_lock);
				STP_PSM_PR_DBG("wakeup +wake_lock(%d)#\n",
						 osal_wake_lock_count(&stp_psm->wake_lock));

				STP_PSM_PR_DBG("mt_combo_plt_exit_deep_idle++\n");
				if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
					mt_combo_plt_exit_deep_idle(COMBO_IF_BTIF);
				else {
					switch (wmt_plat_get_comm_if_type()) {
					case STP_UART_IF_TX:
						mt_combo_plt_exit_deep_idle(COMBO_IF_UART);
						break;
					case STP_SDIO_IF_TX:
						mt_combo_plt_exit_deep_idle(COMBO_IF_MSDC);
						break;
					default:
						break;
					}
				}
				STP_PSM_PR_DBG("mt_combo_plt_exit_deep_idle--\n");

				ret = stp_psm->wmt_notify(WAKEUP);
				if (!ret)
					_stp_psm_wait_wmt_event_wq(stp_psm);
				else
					STP_PSM_PR_ERR("stp_psm->wmt_notify return fail\n");
			} else {
				STP_PSM_PR_ERR("stp_psm->wmt_notify = NULL\n");
				ret = STP_PSM_OPERATION_FAIL;
			}
		} else if (action == HOST_AWAKE) {
			_stp_psm_set_state(stp_psm, INACT_ACT);

			if (stp_psm->wmt_notify) {
				STP_PSM_PR_DBG("host awake +wake_lock(%d)\n",
						 osal_wake_lock_count(&stp_psm->wake_lock));
				osal_wake_lock(&stp_psm->wake_lock);
				STP_PSM_PR_DBG("host awake +wake_lock(%d)#\n",
						 osal_wake_lock_count(&stp_psm->wake_lock));

				STP_PSM_PR_DBG("mt_combo_plt_exit_deep_idle++\n");
				if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC)
					mt_combo_plt_exit_deep_idle(COMBO_IF_BTIF);
				else {
					switch (wmt_plat_get_comm_if_type()) {
					case STP_UART_IF_TX:
						mt_combo_plt_exit_deep_idle(COMBO_IF_UART);
						break;
					case STP_SDIO_IF_TX:
						mt_combo_plt_exit_deep_idle(COMBO_IF_MSDC);
						break;
					default:
						break;
					}
				}
				STP_PSM_PR_DBG("mt_combo_plt_exit_deep_idle--\n");

				ret = stp_psm->wmt_notify(HOST_AWAKE);
				if (!ret)
					_stp_psm_wait_wmt_event_wq(stp_psm);
				else
					STP_PSM_PR_ERR("stp_psm->wmt_notify return fail\n");
			} else {
				STP_PSM_PR_ERR("stp_psm->wmt_notify = NULL\n");
				ret = STP_PSM_OPERATION_FAIL;
			}
		} else if (action == SLEEP) {
			STP_PSM_PR_INFO("In INACT state, dont do SLEEP again\n");
		} else {
			STP_PSM_PR_ERR("Action = %s, state = %d, flag = %ld, abnormal!\n",
					g_psm_action[action],
					stp_psm->work_state, stp_psm->flag.data);
			_stp_psm_dbg_out_printk(g_stp_psm_dbg);
			ret = STP_PSM_OPERATION_FAIL;
		}

		break;

	default:

		/*invalid */
		STP_PSM_PR_ERR("Action = %s, state = %d, flag = %ld, abnormal!\n",
				g_psm_action[action],
				stp_psm->work_state, stp_psm->flag.data);
		_stp_psm_dbg_out_printk(g_stp_psm_dbg);
		ret = STP_PSM_OPERATION_FAIL;

		break;
	}
	return ret;
}

static inline VOID _stp_psm_stp_is_idle(timer_handler_arg arg)
{
	ULONG data;
	MTKSTP_PSM_T *stp_psm;

	GET_HANDLER_DATA(arg, data);
	stp_psm = (MTKSTP_PSM_T *) data;
	osal_clear_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR_RX_HIGH_DENSITY, &stp_psm->flag);
	_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
	osal_clear_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR_TX_HIGH_DENSITY, &stp_psm->flag);
	_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);

	if (osal_test_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR, &stp_psm->flag)) {
		STP_PSM_PR_DBG("STP-PSM DISABLE!\n");
		return;
	}

	if (stp_traffic_start != stp_traffic_current) {
		STP_PSM_PR_DBG("Timer extension due to ongoing traffic (%d, %d)\n",
					stp_traffic_start, stp_traffic_current);
		stp_psm_start_monitor(stp_psm);
		return;
	}

	STP_PSM_PR_DBG("**IDLE is over %d msec, go to sleep!!!**\n", stp_psm->idle_time_to_sleep);
	osal_ftrace_print("**IDLE is over %d msec, go to sleep!!!**\n", stp_psm->idle_time_to_sleep);
	_stp_psm_notify_wmt_sleep_wq(stp_psm);
}

static inline INT32 _stp_psm_init_monitor(MTKSTP_PSM_T *stp_psm)
{
	if (!stp_psm)
		return STP_PSM_OPERATION_FAIL;
	STP_PSM_PR_DBG("init monitor\n");
	stp_psm->psm_timer.timeoutHandler = _stp_psm_stp_is_idle;
	stp_psm->psm_timer.timeroutHandlerData = (ULONG)stp_psm;
	osal_timer_create(&stp_psm->psm_timer);
	return STP_PSM_OPERATION_SUCCESS;
}

static inline INT32 _stp_psm_deinit_monitor(MTKSTP_PSM_T *stp_psm)
{
	if (!stp_psm)
		return STP_PSM_OPERATION_FAIL;
	STP_PSM_PR_INFO("deinit monitor\n");
	osal_timer_stop_sync(&stp_psm->psm_timer);
	return 0;
}
static inline INT32 _stp_psm_is_to_block_traffic(MTKSTP_PSM_T *stp_psm)
{
	INT32 iRet = -1;
	/* osal_lock_unsleepable_lock(&stp_psm->flagSpinlock); */
	if (osal_test_bit(STP_PSM_BLOCK_DATA_EN, &stp_psm->flag))
		iRet = 1;
	else
		iRet = 0;
	/* osal_unlock_unsleepable_lock(&stp_psm->flagSpinlock); */
	return iRet;
}

static inline INT32 _stp_psm_is_disable(MTKSTP_PSM_T *stp_psm)
{
	if (osal_test_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR, &stp_psm->flag))
		return 1;
	return 0;
}

static inline INT32 _stp_psm_do_wait(MTKSTP_PSM_T *stp_psm, MTKSTP_PSM_STATE_T state)
{
#define POLL_WAIT 20		/* 200 */
#define POLL_WAIT_TIME 2000
	INT32 i = 0;
	INT32 limit = POLL_WAIT_TIME / POLL_WAIT;
	UINT64 sec = 0;
	ULONG usec = 0;

	if (state < 0 || state >= STP_PSM_MAX_STATE)
		return STP_PSM_OPERATION_FAIL;

	osal_get_local_time(&sec, &usec);
	while (_stp_psm_get_state(stp_psm) != state && i < limit && mtk_wcn_stp_is_enable()) {
		i++;
		if (i < 3)
			STP_PSM_PR_INFO("STP is waiting state for %s, i=%d, state = %d\n",
					  g_psm_state[state], i, _stp_psm_get_state(stp_psm));
		osal_sleep_ms(POLL_WAIT);
		if (i == 10) {
			STP_PSM_PR_WARN("-Wait for %s takes %d msec\n", g_psm_state[state], i * POLL_WAIT);
			_stp_psm_opid_dbg_out_printk(g_stp_psm_opid_dbg);
		}
	}
	if (mtk_wcn_stp_is_enable() == 0) {
		STP_PSM_PR_INFO("STP disable, maybe do chip reset");
		return STP_PSM_OPERATION_FAIL;
	}

	if (i == limit) {
		STP_PSM_PR_WARN("-Wait for %s takes %llu usec\n", g_psm_state[state], osal_elapsed_us(sec, usec));
		mtk_wcn_wmt_dump_wmtd_backtrace();
		_stp_psm_opid_dbg_out_printk(g_stp_psm_opid_dbg);
		return STP_PSM_OPERATION_FAIL;
	}
	if (i > 0)
		STP_PSM_PR_INFO("+Total waits for %s takes %llu usec\n",
					g_psm_state[state], osal_elapsed_us(sec, usec));
	return STP_PSM_OPERATION_SUCCESS;
}

static inline INT32 _stp_psm_do_wakeup(MTKSTP_PSM_T *stp_psm)
{
	INT32 ret = 0;
	INT32 retry = 10;
	P_OSAL_OP_Q pOpQ;
	P_OSAL_OP pOp;

	STP_PSM_PR_LOUD("*** Do Force Wakeup!***\n\r");
	/* <1>If timer is active, we will stop it. */
	_stp_psm_stop_monitor(stp_psm);
	osal_lock_unsleepable_lock(&(stp_psm->wq_spinlock));
	pOpQ = &stp_psm->rFreeOpQ;
	while (!RB_EMPTY(&stp_psm->rActiveOpQ)) {
		RB_GET(&stp_psm->rActiveOpQ, pOp);
		if (pOp != NULL && !RB_FULL(pOpQ))
			RB_PUT(pOpQ, pOp);
		else
			STP_PSM_PR_ERR("clear up active queue fail, freeQ full\n");
	}
	osal_unlock_unsleepable_lock(&(stp_psm->wq_spinlock));
	/* <5>We issue wakeup request into op queue. and wait for active. */
	do {
		ret = _stp_psm_notify_wmt_wakeup_wq(stp_psm);
		if (ret == STP_PSM_OPERATION_SUCCESS) {
			ret = _stp_psm_do_wait(stp_psm, ACT);
			/* STP_PSM_PR_INFO("<< wait ret = %d, num of activeQ = %d\n", ret,
			 * RB_COUNT(&stp_psm->rActiveOpQ));
			 */
			if (ret == STP_PSM_OPERATION_SUCCESS)
				break;
		} else
			STP_PSM_PR_ERR("_stp_psm_notify_wmt_wakeup_wq fail, retry = %d !!\n", retry);
		/* STP_PSM_PR_INFO("retry = %d\n", retry); */
		retry--;
		if (retry == 0)
			break;
	} while (1);
	if (retry == 0)
		return STP_PSM_OPERATION_FAIL;
	return STP_PSM_OPERATION_SUCCESS;
}

static inline INT32 _stp_psm_disable(MTKSTP_PSM_T *stp_psm)
{
	INT32 ret = STP_PSM_OPERATION_FAIL;
	P_OSAL_THREAD psm_thread;

	STP_PSM_PR_DBG("PSM Disable start\n\r");
	ret = osal_lock_sleepable_lock(&stp_psm->user_lock);
	if (ret) {
		STP_PSM_PR_ERR("--->lock stp_psm->user_lock failed, ret=%d\n", ret);
		return ret;
	}
	osal_set_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR, &stp_psm->flag);
	_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
	ret = _stp_psm_do_wakeup(stp_psm);
	osal_unlock_sleepable_lock(&stp_psm->user_lock);
	if (ret == STP_PSM_OPERATION_SUCCESS)
		STP_PSM_PR_DBG("PSM Disable Success\n");
	else {
		STP_PSM_PR_ERR("***PSM Disable Fail***\n");
		psm_thread = &stp_psm_i.PSMd;
		osal_thread_show_stack(psm_thread);
	}
	return ret;
}

static inline INT32 _stp_psm_enable(MTKSTP_PSM_T *stp_psm, INT32 idle_time_to_sleep)
{
	INT32 ret = STP_PSM_OPERATION_FAIL;

	STP_PSM_PR_LOUD("PSM Enable start\n\r");
	ret = osal_lock_sleepable_lock(&stp_psm->user_lock);
	if (ret) {
		STP_PSM_PR_ERR("--->lock stp_psm->user_lock failed, ret=%d\n", ret);
		return ret;
	}
	osal_set_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR, &stp_psm->flag);
	_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
	ret = _stp_psm_do_wakeup(stp_psm);
	if (ret == STP_PSM_OPERATION_SUCCESS) {
		osal_clear_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR, &stp_psm->flag);
		_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
		stp_psm->idle_time_to_sleep = idle_time_to_sleep;
		if (osal_wake_lock_count(&stp_psm->wake_lock) == 0) {
			STP_PSM_PR_DBG("psm_en+wake_lock(%d)\n",
					 osal_wake_lock_count(&stp_psm->wake_lock));
			osal_wake_lock(&stp_psm->wake_lock);
			STP_PSM_PR_DBG("psm_en+wake_lock(%d)#\n",
					 osal_wake_lock_count(&stp_psm->wake_lock));
		}
		_stp_psm_start_monitor(stp_psm);
		STP_PSM_PR_DBG("PSM Enable succeed\n\r");
	} else
		STP_PSM_PR_ERR("***PSM Enable Fail***\n");
	osal_unlock_sleepable_lock(&stp_psm->user_lock);
	return ret;
}

INT32 _stp_psm_thread_lock_aquire(MTKSTP_PSM_T *stp_psm)
{
	return osal_lock_sleepable_lock(&stp_psm->stp_psm_lock);
}

INT32 _stp_psm_thread_lock_release(MTKSTP_PSM_T *stp_psm)
{
	osal_unlock_sleepable_lock(&stp_psm->stp_psm_lock);
	return 0;
}

MTK_WCN_BOOL _stp_psm_is_quick_ps_support(VOID)
{
	if (stp_psm->is_wmt_quick_ps_support)
		return (*(stp_psm->is_wmt_quick_ps_support)) ();
	STP_PSM_PR_DBG("stp_psm->is_wmt_quick_ps_support is NULL, return false\n\r");
	return MTK_WCN_BOOL_FALSE;
}


MTK_WCN_BOOL stp_psm_is_quick_ps_support(VOID)
{
	return _stp_psm_is_quick_ps_support();
}

#if PSM_USE_COUNT_PACKAGE
INT32 stp_psm_disable_by_tx_rx_density(MTKSTP_PSM_T *stp_psm, INT32 dir)
{
	/* easy the variable maintain beween stp tx, rx thread. */
	/* so we create variable for tx, rx respectively. */
	static INT32 tx_cnt;
	static INT32 rx_cnt;
	INT32 tx_cnt_th = MTK_COMBO_PSM_TX_TH_DEFAULT;
	INT32 rx_cnt_th = MTK_COMBO_PSM_RX_TH_DEFAULT;
	static INT32 is_tx_first = 1;
	static INT32 is_rx_first = 1;
	static ULONG  tx_end_time;
	static ULONG rx_end_time;
	long res;

	/*  */
	/* BT A2DP                  TX CNT = 220, RX CNT = 843 */
	/* BT FTP Transferring  TX CNT = 574, RX CNT = 2233 (1228~1588) */
	/* BT FTP Receiving      TX CNT = 204, RX CNT = 3301 (2072~2515) */
	/* BT OPP  Tx               TX_CNT= 330, RX CNT = 1300~1800 */
	/* BT OPP  Rx               TX_CNT= (109~157), RX CNT = 1681~2436 */
/*    #if defined(MTK_COMBO_PSM_RX_TH)
*    osal_strtol(MTK_COMBO_PSM_RX_TH, 10, &res);
*    rx_cnt_th = (INT32)res;
*    #endif
*    #if defined(MTK_COMBO_PSM_TX_TH)
*    osal_strtol(MTK_COMBO_PSM_TX_TH, 10, &res);
*    tx_cnt_th = (INT32)res;
*    #endif
*/
	STP_PSM_PR_DBG("RX TH:%d; TX TH:%d\n\r", rx_cnt_th, tx_cnt_th);
	stp_traffic_current++;
	if (dir == 0) {		/* tx */
		tx_cnt++;
		if (((long)jiffies - (long)tx_end_time >= 0) || (is_tx_first)) {
			tx_end_time = jiffies + (3 * HZ);
			STP_PSM_PR_INFO("tx cnt = %d in the previous 3 sec,tx_th = %d\n", tx_cnt,
					  tx_cnt_th);
			/* if(tx_cnt > 400)//for high traffic , not to do sleep. */
			if (tx_cnt > tx_cnt_th) {
				osal_set_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR_TX_HIGH_DENSITY,
					     &stp_psm->flag);
				_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
				stp_psm_start_monitor(stp_psm);
			} else {
				osal_clear_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR_TX_HIGH_DENSITY,
					       &stp_psm->flag);
				_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			}
			tx_cnt = 0;
			if (is_tx_first)
				is_tx_first = 0;
		}
	} else {
		rx_cnt++;

		if (((long)jiffies - (long)rx_end_time >= 0) || (is_rx_first)) {
			rx_end_time = jiffies + (3 * HZ);
			STP_PSM_PR_INFO("rx cnt = %d in the previous 3 sec, rx_th = %d\n", rx_cnt,
					  rx_cnt_th);

			/* if(rx_cnt > 2000)//for high traffic , not to do sleep. */
			if (rx_cnt > rx_cnt_th) {	/* for high traffic , not to do sleep. */
				osal_set_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR_RX_HIGH_DENSITY,
					     &stp_psm->flag);
				_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
				stp_psm_start_monitor(stp_psm);
			} else {
				osal_clear_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR_RX_HIGH_DENSITY,
					       &stp_psm->flag);
				_stp_psm_dbg_dmp_in(g_stp_psm_dbg, stp_psm->flag.data, __LINE__);
			}
			rx_cnt = 0;
			if (is_rx_first)
				is_rx_first = 0;
		}
	}

	return 0;
}
#else
static struct timeval tv_now, tv_end;
static INT32 sample_start;
static INT32 tx_sum_len;
static INT32 rx_sum_len;

INT32 stp_psm_disable_by_tx_rx_density(MTKSTP_PSM_T *stp_psm, INT32 dir, INT32 length)
{
	stp_traffic_current++;
	if (sample_start) {
		if (dir)
			rx_sum_len += length;
		else
			tx_sum_len += length;

		osal_do_gettimeofday(&tv_now);
		/* STP_PSM_PR_INFO("tv_now:%d.%d tv_end:%d.%d\n", tv_now.tv_sec, tv_now.tv_usec,
		 * tv_end.tv_sec,tv_end.tv_usec);
		 */
		if (((tv_now.tv_sec == tv_end.tv_sec) && (tv_now.tv_usec > tv_end.tv_usec)) ||
		    (tv_now.tv_sec > tv_end.tv_sec)) {
			STP_PSM_PR_INFO("STP speed rx:%d tx:%d\n", rx_sum_len, tx_sum_len);
			if ((rx_sum_len + tx_sum_len) > RTX_SPEED_THRESHOLD) {
				STP_PSM_PR_INFO("High speed,Disable monitor\n");
				osal_set_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR_TX_HIGH_DENSITY, &stp_psm->flag);
				stp_psm->idle_time_to_sleep = STP_PSM_IDLE_TIME_SLEEP_1000;
				stp_psm_start_monitor(stp_psm);
			} else {
				STP_PSM_PR_INFO("Low speed,Enable monitor\n");
				stp_psm->idle_time_to_sleep = STP_PSM_IDLE_TIME_SLEEP;
				osal_clear_bit(STP_PSM_WMT_EVENT_DISABLE_MONITOR_TX_HIGH_DENSITY, &stp_psm->flag);
			}
			wmt_lib_ps_set_idle_time(stp_psm->idle_time_to_sleep);
			sample_start = 0;
			rx_sum_len = 0;
			tx_sum_len = 0;
		}
	} else {
		sample_start = 1;
		osal_do_gettimeofday(&tv_now);
		tv_end = tv_now;
		tv_end.tv_sec += SAMPLE_DURATION;
	}

	return 0;
}
#endif

/*external function for WMT module to do sleep/wakeup*/
INT32 stp_psm_set_state(MTKSTP_PSM_T *stp_psm, MTKSTP_PSM_STATE_T state)
{
	return _stp_psm_set_state(stp_psm, state);
}


INT32 stp_psm_thread_lock_aquire(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_thread_lock_aquire(stp_psm);
}


INT32 stp_psm_thread_lock_release(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_thread_lock_release(stp_psm);
}



INT32 stp_psm_do_wakeup(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_do_wakeup(stp_psm);
}

INT32 stp_psm_notify_stp(MTKSTP_PSM_T *stp_psm, const MTKSTP_PSM_ACTION_T action)
{

	return _stp_psm_notify_stp(stp_psm, action);
}

INT32 stp_psm_notify_wmt_wakeup(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_notify_wmt_wakeup_wq(stp_psm);
}

INT32 stp_psm_notify_wmt_sleep(MTKSTP_PSM_T *stp_psm)
{

	return _stp_psm_notify_wmt_sleep_wq(stp_psm);
}

INT32 stp_psm_start_monitor(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_start_monitor(stp_psm);
}

INT32 stp_psm_is_to_block_traffic(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_is_to_block_traffic(stp_psm);
}

INT32 stp_psm_is_disable(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_is_disable(stp_psm);
}

INT32 stp_psm_has_pending_data(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_has_pending_data(stp_psm);
}

INT32 stp_psm_release_data(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_release_data(stp_psm);
}

INT32
stp_psm_hold_data(MTKSTP_PSM_T *stp_psm, const PUINT8 buffer, const UINT32 len, const UINT8 type)
{
	return _stp_psm_hold_data(stp_psm, buffer, len, type);
}

INT32 stp_psm_disable(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_disable(stp_psm);
}

INT32 stp_psm_enable(MTKSTP_PSM_T *stp_psm, INT32 idle_time_to_sleep)
{
	return _stp_psm_enable(stp_psm, idle_time_to_sleep);
}

INT32 stp_psm_reset(MTKSTP_PSM_T *stp_psm)
{
	stp_psm_set_sleep_enable(stp_psm);

	return _stp_psm_reset(stp_psm);
}

INT32 stp_psm_sleep_for_thermal(MTKSTP_PSM_T *stp_psm)
{
	return _stp_psm_notify_wmt_sleep_wq(stp_psm);
}


INT32 stp_psm_set_sleep_enable(MTKSTP_PSM_T *stp_psm)
{
	INT32 ret = 0;

	if (stp_psm) {
		stp_psm->sleep_en = 1;
		STP_PSM_PR_DBG("\n");
		ret = 0;
	} else {
		STP_PSM_PR_INFO("Null pointer\n");
		ret = -1;
	}

	return ret;
}


INT32 stp_psm_set_sleep_disable(MTKSTP_PSM_T *stp_psm)
{
	INT32 ret = 0;

	if (stp_psm) {
		stp_psm->sleep_en = 0;
		STP_PSM_PR_DBG("\n");
		ret = 0;
	} else {
		STP_PSM_PR_INFO("Null pointer\n");
		ret = -1;
	}

	return ret;
}


/* stp_psm_check_sleep_enable  - to check if sleep cmd is enabled or not
 * @ stp_psm - pointer of psm
 *
 * return 1 if sleep is enabled; else return 0 if disabled; else error code
 */
INT32 stp_psm_check_sleep_enable(MTKSTP_PSM_T *stp_psm)
{
	INT32 ret = 0;

	if (stp_psm) {
		ret = stp_psm->sleep_en;
		STP_PSM_PR_DBG("%s\n", ret ? "enabled" : "disabled");
	} else {
		STP_PSM_PR_INFO("Null pointer\n");
		ret = -1;
	}

	return ret;
}

static INT32 _stp_psm_dbg_dmp_in(STP_PSM_RECORD_T *stp_psm_dbg, UINT32 flag, UINT32 line_num)
{
	INT32 index = 0;
	struct timeval now;

	if (stp_psm_dbg) {
		osal_lock_unsleepable_lock(&stp_psm_dbg->lock);
		osal_do_gettimeofday(&now);
		index = stp_psm_dbg->in - 1;
		index = (index + STP_PSM_DBG_SIZE) % STP_PSM_DBG_SIZE;
		STP_PSM_PR_DBG("index(%d)\n", index);
		stp_psm_dbg->queue[stp_psm_dbg->in].prev_flag = stp_psm_dbg->queue[index].cur_flag;
		stp_psm_dbg->queue[stp_psm_dbg->in].cur_flag = flag;
		stp_psm_dbg->queue[stp_psm_dbg->in].line_num = line_num;
		stp_psm_dbg->queue[stp_psm_dbg->in].package_no = g_record_num++;
		stp_psm_dbg->queue[stp_psm_dbg->in].sec = now.tv_sec;
		stp_psm_dbg->queue[stp_psm_dbg->in].usec = now.tv_usec;
		stp_psm_dbg->size++;
		STP_PSM_PR_DBG("pre_Flag = %d, cur_flag = %d\n", stp_psm_dbg->queue[stp_psm_dbg->in].prev_flag,
				 stp_psm_dbg->queue[stp_psm_dbg->in].cur_flag);
		stp_psm_dbg->size = (stp_psm_dbg->size > STP_PSM_DBG_SIZE) ? STP_PSM_DBG_SIZE : stp_psm_dbg->size;
		stp_psm_dbg->in = (stp_psm_dbg->in >= (STP_PSM_DBG_SIZE - 1)) ? (0) : (stp_psm_dbg->in + 1);
		STP_PSM_PR_DBG("record size = %d, in = %d num = %d\n", stp_psm_dbg->size, stp_psm_dbg->in, line_num);

		osal_unlock_unsleepable_lock(&stp_psm_dbg->lock);
	}
	return 0;
}

static INT32 _stp_psm_dbg_out_printk(STP_PSM_RECORD_T *stp_psm_dbg)
{

	UINT32 dumpSize = 0;
	UINT32 inIndex = 0;
	UINT32 outIndex = 0;

	if (!stp_psm_dbg) {
		STP_PSM_PR_ERR("NULL g_stp_psm_dbg reference\n");
		return -1;
	}
	osal_lock_unsleepable_lock(&stp_psm_dbg->lock);

	inIndex = stp_psm_dbg->in;
	dumpSize = stp_psm_dbg->size;
	if (dumpSize == STP_PSM_DBG_SIZE)
		outIndex = inIndex;
	else
		outIndex = ((inIndex + STP_PSM_DBG_SIZE) - dumpSize) % STP_PSM_DBG_SIZE;

	STP_PSM_PR_INFO("loged record size = %d, in(%d), out(%d)\n", dumpSize, inIndex, outIndex);
	while (dumpSize > 0) {

		pr_info("STP-PSM:%d.%ds, n(%d)pre_flag(%d)cur_flag(%d)line_no(%d)\n",
		       stp_psm_dbg->queue[outIndex].sec,
		       stp_psm_dbg->queue[outIndex].usec,
		       stp_psm_dbg->queue[outIndex].package_no,
		       stp_psm_dbg->queue[outIndex].prev_flag,
		       stp_psm_dbg->queue[outIndex].cur_flag, stp_psm_dbg->queue[outIndex].line_num);

		outIndex = (outIndex >= (STP_PSM_DBG_SIZE - 1)) ? (0) : (outIndex + 1);
		dumpSize--;

	}

	osal_unlock_unsleepable_lock(&stp_psm_dbg->lock);

	return 0;
}

static INT32 _stp_psm_opid_dbg_dmp_in(P_STP_PSM_OPID_RECORD p_opid_dbg, UINT32 opid, UINT32 line_num)
{
	INT32 index = 0;
	struct timeval now;
	UINT64 ts;
	ULONG nsec;

	osal_get_local_time(&ts, &nsec);
	if (p_opid_dbg) {
		osal_lock_unsleepable_lock(&p_opid_dbg->lock);
		osal_do_gettimeofday(&now);
		index = p_opid_dbg->in - 1;
		index = (index + STP_PSM_DBG_SIZE) % STP_PSM_DBG_SIZE;
		STP_PSM_PR_DBG("index(%d)\n", index);
		p_opid_dbg->queue[p_opid_dbg->in].prev_flag = p_opid_dbg->queue[index].cur_flag;
		p_opid_dbg->queue[p_opid_dbg->in].cur_flag = opid;
		p_opid_dbg->queue[p_opid_dbg->in].line_num = line_num;
		p_opid_dbg->queue[p_opid_dbg->in].package_no = g_opid_record_num++;
		p_opid_dbg->queue[p_opid_dbg->in].sec = now.tv_sec;
		p_opid_dbg->queue[p_opid_dbg->in].usec = now.tv_usec;
		p_opid_dbg->queue[p_opid_dbg->in].pid = current->pid;
		p_opid_dbg->queue[p_opid_dbg->in].l_sec = ts;
		p_opid_dbg->queue[p_opid_dbg->in].l_nsec = nsec;
		p_opid_dbg->size++;
		STP_PSM_PR_DBG("pre_opid = %d, cur_opid = %d\n", p_opid_dbg->queue[p_opid_dbg->in].prev_flag,
				 p_opid_dbg->queue[p_opid_dbg->in].cur_flag);
		p_opid_dbg->size = (p_opid_dbg->size > STP_PSM_DBG_SIZE) ? STP_PSM_DBG_SIZE : p_opid_dbg->size;
		p_opid_dbg->in = (p_opid_dbg->in >= (STP_PSM_DBG_SIZE - 1)) ? (0) : (p_opid_dbg->in + 1);
		STP_PSM_PR_DBG("opid record size = %d, in = %d num = %d\n", p_opid_dbg->size, p_opid_dbg->in,
				 line_num);

		osal_unlock_unsleepable_lock(&p_opid_dbg->lock);
	}
	return 0;

}

static INT32 _stp_psm_opid_dbg_out_printk(P_STP_PSM_OPID_RECORD p_opid_dbg)
{
	UINT32 dumpSize = 0;
	UINT32 inIndex = 0;
	UINT32 outIndex = 0;

	if (!p_opid_dbg) {
		STP_PSM_PR_ERR("NULL p_opid_dbg reference\n");
		return -1;
	}
	osal_lock_unsleepable_lock(&p_opid_dbg->lock);

	inIndex = p_opid_dbg->in;
	dumpSize = p_opid_dbg->size;
	if (dumpSize == STP_PSM_DBG_SIZE)
		outIndex = inIndex;
	else
		outIndex = ((inIndex + STP_PSM_DBG_SIZE) - dumpSize) % STP_PSM_DBG_SIZE;

	STP_PSM_PR_INFO("loged record size = %d, in(%d), out(%d)\n", dumpSize, inIndex, outIndex);
	while (dumpSize > 0) {

		pr_info("STP-PSM:%d.%ds, time[%llu.%06lu], n(%d)pre_flag(%d)cur_flag(%d)line_no(%d) pid(%d)\n",
		       p_opid_dbg->queue[outIndex].sec,
		       p_opid_dbg->queue[outIndex].usec,
		       p_opid_dbg->queue[outIndex].l_sec,
		       p_opid_dbg->queue[outIndex].l_nsec,
		       p_opid_dbg->queue[outIndex].package_no,
		       p_opid_dbg->queue[outIndex].prev_flag,
		       p_opid_dbg->queue[outIndex].cur_flag,
		       p_opid_dbg->queue[outIndex].line_num, p_opid_dbg->queue[outIndex].pid);

		outIndex = (outIndex >= (STP_PSM_DBG_SIZE - 1)) ? (0) : (outIndex + 1);
		dumpSize--;

	}

	osal_unlock_unsleepable_lock(&p_opid_dbg->lock);

	return 0;

}

VOID stp_psm_print_op_history(VOID)
{
	osal_op_history_print(&stp_psm->op_history, "_stp_psm_proc");
}

MTKSTP_PSM_T *stp_psm_init(VOID)
{
	INT32 err = 0;
	INT32 i = 0;
	INT32 ret = -1;

	STP_PSM_PR_DBG("psm init\n");

	stp_psm->work_state = ACT;
	stp_psm->wmt_notify = wmt_lib_ps_stp_cb;
	stp_psm->is_wmt_quick_ps_support = wmt_lib_is_quick_ps_support;
	stp_psm->idle_time_to_sleep = STP_PSM_IDLE_TIME_SLEEP;
	stp_psm->update_wmt_fw_patch_chip_rst = wmt_lib_update_fw_patch_chip_rst;
	stp_psm->flag.data = 0;
	stp_psm->stp_tx_cb = NULL;
	stp_psm_set_sleep_enable(stp_psm);

	ret = osal_fifo_init(&stp_psm->hold_fifo, NULL, STP_PSM_FIFO_SIZE);
	if (ret < 0) {
		STP_PSM_PR_ERR("FIFO INIT FAILS\n");
		goto ERR_EXIT4;
	}

	osal_fifo_reset(&stp_psm->hold_fifo);
	osal_sleepable_lock_init(&stp_psm->user_lock);
	/*psm_fifo_lock_init(stp_psm);*/
	osal_sleepable_lock_init(&stp_psm->hold_fifo_spinlock_global);
	osal_unsleepable_lock_init(&stp_psm->wq_spinlock);
	osal_sleepable_lock_init(&stp_psm->stp_psm_lock);

/* osal_unsleepable_lock_init(&stp_psm->flagSpinlock); */

	osal_memcpy(stp_psm->wake_lock.name, "MT662x", 6);
	stp_psm->wake_lock.init_flag = 0;
	osal_wake_lock_init(&stp_psm->wake_lock);
	osal_event_init(&stp_psm->STPd_event);
	RB_INIT(&stp_psm->rFreeOpQ, STP_OP_BUF_SIZE);
	RB_INIT(&stp_psm->rActiveOpQ, STP_OP_BUF_SIZE);
	/* Put all to free Q */
	for (i = 0; i < STP_OP_BUF_SIZE; i++) {
		osal_signal_init(&(stp_psm->arQue[i].signal));
		_stp_psm_put_op(stp_psm, &stp_psm->rFreeOpQ, &(stp_psm->arQue[i]));
	}
	/* stp_psm->current_active_op = NULL; */
	stp_psm->last_active_opId = STP_OPID_PSM_INALID;
	osal_op_history_init(&stp_psm->op_history, 16);
	/*Generate PSM thread, to servie STP-CORE and WMT-CORE for sleeping, waking up and host awake */
	stp_psm->PSMd.pThreadData = (PVOID) stp_psm;
	stp_psm->PSMd.pThreadFunc = (PVOID) _stp_psm_proc;
	osal_memcpy(stp_psm->PSMd.threadName, PSM_THREAD_NAME, osal_strlen(PSM_THREAD_NAME));

	ret = osal_thread_create(&stp_psm->PSMd);
	if (ret < 0) {
		STP_PSM_PR_ERR("osal_thread_create fail...\n");
		goto ERR_EXIT5;
	}
	/* init_waitqueue_head(&stp_psm->wait_wmt_q); */
	stp_psm->wait_wmt_q.timeoutValue = STP_PSM_WAIT_EVENT_TIMEOUT;
	osal_event_init(&stp_psm->wait_wmt_q);

	err = _stp_psm_init_monitor(stp_psm);
	if (err) {
		STP_PSM_PR_ERR("__stp_psm_init ERROR\n");
		goto ERR_EXIT6;
	}
	/* Start STPd thread */
	ret = osal_thread_run(&stp_psm->PSMd);
	if (ret < 0) {
		STP_PSM_PR_ERR("osal_thread_run FAILS\n");
		goto ERR_EXIT6;
	}

	g_stp_psm_dbg = (STP_PSM_RECORD_T *) osal_malloc(osal_sizeof(STP_PSM_RECORD_T));
	if (!g_stp_psm_dbg) {
		STP_PSM_PR_ERR("stp psm dbg allocate memory fail!\n");
		return NULL;
	}
	osal_memset(g_stp_psm_dbg, 0, osal_sizeof(STP_PSM_RECORD_T));
	osal_unsleepable_lock_init(&g_stp_psm_dbg->lock);

	g_stp_psm_opid_dbg = (STP_PSM_OPID_RECORD *) osal_malloc(osal_sizeof(STP_PSM_OPID_RECORD));
	if (!g_stp_psm_opid_dbg) {
		STP_PSM_PR_ERR("stp psm dbg allocate memory fail!\n");
		return NULL;
	}
	osal_memset(g_stp_psm_opid_dbg, 0, osal_sizeof(STP_PSM_OPID_RECORD));
	osal_unsleepable_lock_init(&g_stp_psm_opid_dbg->lock);

	return stp_psm;

ERR_EXIT6:

	ret = osal_thread_destroy(&stp_psm->PSMd);
	if (ret < 0) {
		STP_PSM_PR_ERR("osal_thread_destroy FAILS\n");
		goto ERR_EXIT5;
	}
ERR_EXIT5:
	osal_fifo_deinit(&stp_psm->hold_fifo);
ERR_EXIT4:

	return NULL;
}

INT32 stp_psm_deinit(MTKSTP_PSM_T *stp_psm)
{
	INT32 ret = -1;

	STP_PSM_PR_INFO("psm deinit\n");
	if (g_stp_psm_dbg) {
		osal_unsleepable_lock_deinit(&g_stp_psm_dbg->lock);
		osal_free(g_stp_psm_dbg);
		g_stp_psm_dbg = NULL;
	}

	if (!stp_psm)
		return STP_PSM_OPERATION_FAIL;

	ret = osal_thread_destroy(&stp_psm->PSMd);
	if (ret < 0)
		STP_PSM_PR_ERR("osal_thread_destroy FAILS\n");

	ret = _stp_psm_deinit_monitor(stp_psm);
	if (ret < 0)
		STP_PSM_PR_ERR("_stp_psm_deinit_monitor ERROR\n");

	osal_wake_lock_deinit(&stp_psm->wake_lock);
	osal_fifo_deinit(&stp_psm->hold_fifo);
	osal_sleepable_lock_deinit(&stp_psm->user_lock);
	/*psm_fifo_lock_deinit(stp_psm);*/
	osal_sleepable_lock_deinit(&stp_psm->hold_fifo_spinlock_global);
	osal_unsleepable_lock_deinit(&stp_psm->wq_spinlock);
	osal_sleepable_lock_deinit(&stp_psm->stp_psm_lock);
/* osal_unsleepable_lock_deinit(&stp_psm->flagSpinlock); */

	return STP_PSM_OPERATION_SUCCESS;
}
