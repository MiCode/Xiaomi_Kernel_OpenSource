/*
* Copyright (C) 2011-2014 MediaTek Inc.
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

#include <linux/atomic.h>
#include "osal_typedef.h"
#include "osal.h"
#include "stp_dbg.h"
#include "stp_core.h"
#include "btm_core.h"
#include "wmt_plat.h"
#include "wmt_step.h"
#include "wmt_detect.h"
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
#include "connsys_debug_utility.h"
#endif
#include <linux/kthread.h>

#define PFX_BTM                         "[STP-BTM] "
#define STP_BTM_LOG_LOUD                 4
#define STP_BTM_LOG_DBG                  3
#define STP_BTM_LOG_INFO                 2
#define STP_BTM_LOG_WARN                 1
#define STP_BTM_LOG_ERR                  0

INT32 gBtmDbgLevel = STP_BTM_LOG_INFO;

#define STP_BTM_PR_LOUD(fmt, arg...) \
do { \
	if (gBtmDbgLevel >= STP_BTM_LOG_LOUD) \
		pr_info(PFX_BTM "%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_BTM_PR_DBG(fmt, arg...) \
do { \
	if (gBtmDbgLevel >= STP_BTM_LOG_DBG) \
		pr_info(PFX_BTM "%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_BTM_PR_INFO(fmt, arg...) \
do { \
	if (gBtmDbgLevel >= STP_BTM_LOG_INFO) \
		pr_info(PFX_BTM "[I]%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_BTM_PR_WARN(fmt, arg...) \
do { \
	if (gBtmDbgLevel >= STP_BTM_LOG_WARN) \
		pr_warn(PFX_BTM "[W]%s: "  fmt, __func__, ##arg); \
} while (0)
#define STP_BTM_PR_ERR(fmt, arg...) \
do { \
	if (gBtmDbgLevel >= STP_BTM_LOG_ERR) \
		pr_err(PFX_BTM "[E]%s(%d):ERROR! "   fmt, __func__, __LINE__, ##arg); \
} while (0)

#define ASSERT(expr)

MTKSTP_BTM_T stp_btm_i;
MTKSTP_BTM_T *stp_btm = &stp_btm_i;

const PINT8 g_btm_op_name[] = {
	"STP_OPID_BTM_RETRY",
	"STP_OPID_BTM_RST",
	"STP_OPID_BTM_DBG_DUMP",
	"STP_OPID_BTM_DUMP_TIMEOUT",
	"STP_OPID_BTM_POLL_CPUPCR",
	"STP_OPID_BTM_PAGED_DUMP",
	"STP_OPID_BTM_FULL_DUMP",
	"STP_OPID_BTM_PAGED_TRACE",
	"STP_OPID_BTM_FORCE_FW_ASSERT",
#if CFG_WMT_LTE_COEX_HANDLING
	"STP_OPID_BTM_WMT_LTE_COEX",
#endif
	"STP_OPID_BTM_ASSERT_TIMEOUT",
	"STP_OPID_BTM_EMI_DUMP_END",
	"STP_OPID_BTM_EXIT"
};

static VOID stp_btm_trigger_assert_timeout_handler(timer_handler_arg arg)
{
	ULONG data;

	GET_HANDLER_DATA(arg, data);
	if (mtk_wcn_stp_coredump_start_get() == 0)
		stp_btm_notify_assert_timeout_wq((MTKSTP_BTM_T *)data);
}

static INT32 _stp_btm_handler(MTKSTP_BTM_T *stp_btm, P_STP_BTM_OP pStpOp)
{
	INT32 ret = -1;
	/* core dump target, 0: aee; 1: netlink */
	INT32 dump_sink = mtk_wcn_stp_coredump_flag_get();

	if (pStpOp == NULL)
		return -1;

	switch (pStpOp->opId) {
	case STP_OPID_BTM_EXIT:
		/* TODO: clean all up? */
		ret = 0;
		break;

		/*tx timeout retry */
	case STP_OPID_BTM_RETRY:
		if (mtk_wcn_stp_coredump_start_get() == 0)
			stp_do_tx_timeout();
		ret = 0;
		break;

		/*whole chip reset */
	case STP_OPID_BTM_RST:
		STP_BTM_PR_INFO("whole chip reset start!\n");
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC &&
		    mtk_wcn_stp_coredump_flag_get() != 0 && chip_reset_only == 0) {
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
			connsys_dedicated_log_flush_emi();
#endif
			stp_dbg_core_dump_flush(0, MTK_WCN_BOOL_FALSE);
		}
		STP_BTM_PR_INFO("....+\n");
		WMT_STEP_DO_ACTIONS_FUNC(STEP_TRIGGER_POINT_BEFORE_CHIP_RESET);
		if (stp_btm->wmt_notify) {
			stp_btm->wmt_notify(BTM_RST_OP);
			ret = 0;
		} else {
			STP_BTM_PR_ERR("stp_btm->wmt_notify is NULL.");
			ret = -1;
		}

		STP_BTM_PR_INFO("whole chip reset end!\n");
		WMT_STEP_DO_ACTIONS_FUNC(STEP_TRIGGER_POINT_AFTER_CHIP_RESET);
		break;

	case STP_OPID_BTM_DBG_DUMP:
		/*Notify the wmt to get dump data */
		STP_BTM_PR_DBG("wmt dmp notification\n");
		set_user_nice(stp_btm->BTMd.pThread, -20);
		ret = stp_dbg_core_dump(dump_sink);
		set_user_nice(stp_btm->BTMd.pThread, 0);
		break;

	case STP_OPID_BTM_DUMP_TIMEOUT:
		/* append fake coredump end message */
		if (dump_sink == 2 && wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO) {
			stp_dbg_set_coredump_timer_state(CORE_DUMP_DOING);
			STP_BTM_PR_WARN("generate fake coredump message\n");
			stp_dbg_nl_send_data(FAKECOREDUMPEND, osal_sizeof(FAKECOREDUMPEND));
		}
		stp_dbg_poll_cpupcr(5, 1, 1);
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO) {
			/* Flush dump data, and reset compressor */
			STP_BTM_PR_INFO("Flush dump data\n");
			stp_dbg_core_dump_flush(0, MTK_WCN_BOOL_TRUE);
		}
		ret = mtk_wcn_stp_coredump_timeout_handle();
		break;
#if CFG_WMT_LTE_COEX_HANDLING
	case STP_OPID_BTM_WMT_LTE_COEX:
		ret = wmt_idc_msg_to_lte_handing();
		break;
#endif
	case STP_OPID_BTM_ASSERT_TIMEOUT:
		mtk_wcn_stp_assert_timeout_handle();
		ret = 0;
		break;
	case STP_OPID_BTM_EMI_DUMP_END:
		STP_BTM_PR_INFO("emi dump end notification.\n");
		stp_dbg_stop_emi_dump();
		mtk_wcn_stp_ctx_restore();
		ret = 0;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static P_OSAL_OP _stp_btm_get_op(MTKSTP_BTM_T *stp_btm, P_OSAL_OP_Q pOpQ)
{
	P_OSAL_OP pOp;
	/* INT32 ret = 0; */

	if (!pOpQ) {
		STP_BTM_PR_WARN("!pOpQ\n");
		return NULL;
	}

	osal_lock_unsleepable_lock(&(stp_btm->wq_spinlock));
	/* acquire lock success */
	RB_GET(pOpQ, pOp);
	osal_unlock_unsleepable_lock(&(stp_btm->wq_spinlock));

	if (!pOp)
		STP_BTM_PR_DBG("RB_GET fail\n");
	return pOp;
}

static INT32 _stp_btm_put_op(MTKSTP_BTM_T *stp_btm, P_OSAL_OP_Q pOpQ, P_OSAL_OP pOp)
{
	INT32 ret;
	P_OSAL_OP pOp_latest = NULL;
	P_OSAL_OP pOp_current = NULL;
	INT32 flag_latest = 1;
	INT32 flag_current = 1;

	if (!pOpQ || !pOp) {
		STP_BTM_PR_WARN("invalid input param: 0x%p, 0x%p\n", pOpQ, pOp);
		return 0;	/* ;MTK_WCN_BOOL_FALSE; */
	}
	ret = 0;

	osal_lock_unsleepable_lock(&(stp_btm->wq_spinlock));
	/* acquire lock success */
	if (&stp_btm->rFreeOpQ == pOpQ) {
		if (!RB_FULL(pOpQ))
			RB_PUT(pOpQ, pOp);
		else
			ret = -1;
	} else if (pOp->op.opId == STP_OPID_BTM_RST ||
		   pOp->op.opId == STP_OPID_BTM_ASSERT_TIMEOUT ||
		   pOp->op.opId == STP_OPID_BTM_DUMP_TIMEOUT ||
		   pOp->op.opId == STP_OPID_BTM_EMI_DUMP_END) {
		if (!RB_FULL(pOpQ)) {
			RB_PUT(pOpQ, pOp);
			STP_BTM_PR_DBG("RB_PUT: 0x%x\n", pOp->op.opId);
		} else
			ret = -1;
	} else {
		pOp_current = stp_btm_get_current_op(stp_btm);
		if (pOp_current) {
			if (pOp_current->op.opId == STP_OPID_BTM_RST ||
			    pOp_current->op.opId == STP_OPID_BTM_DUMP_TIMEOUT ||
			    (pOp_current->op.opId == STP_OPID_BTM_ASSERT_TIMEOUT &&
			     pOp->op.opId != STP_OPID_BTM_DBG_DUMP)) {
				STP_BTM_PR_DBG("current: 0x%x\n", pOp_current->op.opId);
				flag_current = 0;
			}
		}

		RB_GET_LATEST(pOpQ, pOp_latest);
		if (pOp_latest) {
			if (pOp_latest->op.opId == STP_OPID_BTM_RST ||
			    pOp_latest->op.opId == STP_OPID_BTM_ASSERT_TIMEOUT ||
			    pOp_latest->op.opId == STP_OPID_BTM_DUMP_TIMEOUT) {
				STP_BTM_PR_DBG("latest: 0x%x\n", pOp_latest->op.opId);
				flag_latest = 0;
			}
			if (pOp_latest->op.opId == pOp->op.opId
#if CFG_WMT_LTE_COEX_HANDLING
			    && pOp->op.opId != STP_OPID_BTM_WMT_LTE_COEX
#endif
			) {
				flag_latest = 0;
				STP_BTM_PR_DBG("With the latest a command repeat: latest 0x%x,current 0x%x\n",
						pOp_latest->op.opId, pOp->op.opId);
			}
		}
		if (flag_current && flag_latest) {
			if (!RB_FULL(pOpQ)) {
				RB_PUT(pOpQ, pOp);
				STP_BTM_PR_DBG("RB_PUT: 0x%x\n", pOp->op.opId);
			} else
				ret = -1;
		} else
			ret = 0;

	}

	if (ret) {
		STP_BTM_PR_DBG("RB_FULL(0x%p) %d ,rFreeOpQ = %p, rActiveOpQ = %p\n",
			pOpQ, RB_COUNT(pOpQ), &stp_btm->rFreeOpQ, &stp_btm->rActiveOpQ);
		osal_opq_dump_locked("FreeOpQ", &stp_btm->rFreeOpQ);
		osal_opq_dump_locked("ActiveOpQ", &stp_btm->rActiveOpQ);
	}

	osal_unlock_unsleepable_lock(&(stp_btm->wq_spinlock));
	return ret ? 0 : 1;
}

P_OSAL_OP _stp_btm_get_free_op(MTKSTP_BTM_T *stp_btm)
{
	P_OSAL_OP pOp;

	if (stp_btm) {
		pOp = _stp_btm_get_op(stp_btm, &stp_btm->rFreeOpQ);
		if (pOp) {
			osal_memset(pOp, 0, osal_sizeof(OSAL_OP));
		}

		return pOp;
	} else
		return NULL;
}

INT32 _stp_btm_put_act_op(MTKSTP_BTM_T *stp_btm, P_OSAL_OP pOp)
{
	INT32 bRet = 0;
	INT32 wait_ret = -1;

	P_OSAL_SIGNAL pSignal = NULL;

	if (!stp_btm || !pOp) {
		STP_BTM_PR_ERR("Input NULL pointer\n");
		return bRet;
	}
	do {
		pSignal = &pOp->signal;

		if (pSignal->timeoutValue) {
			pOp->result = -9;
			osal_signal_init(&pOp->signal);
		}

		/* Init ref_count to 2, as one is held by current thread, the second by btm thread */
		atomic_set(&pOp->ref_count, 2);

		/* put to active Q */
		bRet = _stp_btm_put_op(stp_btm, &stp_btm->rActiveOpQ, pOp);
		if (bRet == 0) {
			STP_BTM_PR_DBG("put active queue fail\n");
			atomic_dec(&pOp->ref_count);
			break;
		}
		/* wake up wmtd */
		osal_trigger_event(&stp_btm->STPd_event);

		if (pSignal->timeoutValue == 0) {
			bRet = 1;	/* MTK_WCN_BOOL_TRUE; */
			break;
		}

		/* check result */
		wait_ret = osal_wait_for_signal_timeout(&pOp->signal, &stp_btm->BTMd);

		STP_BTM_PR_DBG("wait completion:%d\n", wait_ret);
		if (!wait_ret) {
			STP_BTM_PR_ERR("wait completion timeout\n");
			/* TODO: how to handle it? retry? */
		} else {
			if (pOp->result)
				STP_BTM_PR_WARN("op(%d) result:%d\n", pOp->op.opId, pOp->result);
			bRet = (pOp->result) ? 0 : 1;
		}
	} while (0);

	if (pOp && atomic_dec_and_test(&pOp->ref_count)) {
		/* put Op back to freeQ */
		_stp_btm_put_op(stp_btm, &stp_btm->rFreeOpQ, pOp);
	}

	return bRet;
}

static INT32 _stp_btm_wait_for_msg(PVOID pvData)
{
	MTKSTP_BTM_T *stp_btm = (MTKSTP_BTM_T *) pvData;

	return (!RB_EMPTY(&stp_btm->rActiveOpQ)) || osal_thread_should_stop(&stp_btm->BTMd);
}

static INT32 _stp_btm_proc(PVOID pvData)
{
	MTKSTP_BTM_T *stp_btm = (MTKSTP_BTM_T *) pvData;
	P_OSAL_OP pOp;
	INT32 id;
	INT32 result;

	if (!stp_btm) {
		STP_BTM_PR_WARN("!stp_btm\n");
		return -1;
	}

	for (;;) {
		pOp = NULL;

		osal_wait_for_event(&stp_btm->STPd_event, _stp_btm_wait_for_msg, (PVOID) stp_btm);

		if (osal_thread_should_stop(&stp_btm->BTMd)) {
			STP_BTM_PR_INFO("should stop now...\n");
			/* TODO: clean up active opQ */
			break;
		}

		if (stp_btm->gDumplogflag) {
			/* pr_warn("enter place1\n"); */
			stp_btm->gDumplogflag = 0;
			continue;
		}

		/* get Op from activeQ */
		pOp = _stp_btm_get_op(stp_btm, &stp_btm->rActiveOpQ);

		if (!pOp) {
			STP_BTM_PR_WARN("get_lxop activeQ fail\n");
			continue;
		}
		osal_op_history_save(&stp_btm->op_history, pOp);

		id = osal_op_get_id(pOp);

		if ((id >= STP_OPID_BTM_NUM) || (id < 0)) {
			STP_BTM_PR_WARN("abnormal opid id: 0x%x\n", id);
			result = -1;
			goto handler_done;
		}

		STP_BTM_PR_DBG("======> lxop_get_opid = %d, %s, remaining count = *%d*\n", id,
				g_btm_op_name[id], RB_COUNT(&stp_btm->rActiveOpQ));

		osal_lock_unsleepable_lock(&(stp_btm->wq_spinlock));
		stp_btm_set_current_op(stp_btm, pOp);
		osal_unlock_unsleepable_lock(&(stp_btm->wq_spinlock));
		result = _stp_btm_handler(stp_btm, &pOp->op);
		osal_lock_unsleepable_lock(&(stp_btm->wq_spinlock));
		stp_btm_set_current_op(stp_btm, NULL);
		osal_unlock_unsleepable_lock(&(stp_btm->wq_spinlock));

		if (result) {
			STP_BTM_PR_WARN("opid id(0x%x)(%s) error(%d)\n", id,
					g_btm_op_name[id], result);
		}

handler_done:

		if (atomic_dec_and_test(&pOp->ref_count)) {
			_stp_btm_put_op(stp_btm, &stp_btm->rFreeOpQ, pOp);
		} else if (osal_op_is_wait_for_signal(pOp)) {
			osal_op_raise_signal(pOp, result);
		}

		if (id == STP_OPID_BTM_EXIT) {
			break;
		} else if (id == STP_OPID_BTM_RST) {
			/* prevent multi reset case */
			stp_btm_reset_btm_wq(stp_btm);
		}
	}

	STP_BTM_PR_INFO("exits\n");

	return 0;
}

static inline INT32 _stp_btm_dump_type(MTKSTP_BTM_T *stp_btm, ENUM_STP_BTM_OPID_T opid)
{
	P_OSAL_OP pOp;
	INT32 bRet;
	INT32 retval;

	pOp = _stp_btm_get_free_op(stp_btm);
	if (!pOp) {
		STP_BTM_PR_WARN("get_free_lxop fail\n");
		return -1;	/* break; */
	}

	pOp->op.opId = opid;
	pOp->signal.timeoutValue = 0;
	bRet = _stp_btm_put_act_op(stp_btm, pOp);
	STP_BTM_PR_DBG("OPID(%d) type(%zd) bRet(%d)\n\n", pOp->op.opId, pOp->op.au4OpData[0], bRet);
	retval = (bRet == 0) ? STP_BTM_OPERATION_FAIL : STP_BTM_OPERATION_SUCCESS;

	return retval;
}

static inline INT32 _stp_btm_notify_wmt_rst_wq(MTKSTP_BTM_T *stp_btm)
{
	INT32 retval;

	if (stp_btm == NULL)
		return STP_BTM_OPERATION_FAIL;

	retval = _stp_btm_dump_type(stp_btm, STP_OPID_BTM_RST);
	return retval;
}

static inline INT32 _stp_btm_notify_stp_retry_wq(MTKSTP_BTM_T *stp_btm)
{
	INT32 retval;

	if (stp_btm == NULL)
		return STP_BTM_OPERATION_FAIL;

	retval = _stp_btm_dump_type(stp_btm, STP_OPID_BTM_RETRY);
	return retval;
}


static inline INT32 _stp_btm_notify_coredump_timeout_wq(MTKSTP_BTM_T *stp_btm)
{
	INT32 retval;

	if (!stp_btm)
		return STP_BTM_OPERATION_FAIL;

	stp_btm_reset_btm_wq(stp_btm);
	retval = _stp_btm_dump_type(stp_btm, STP_OPID_BTM_DUMP_TIMEOUT);
	return retval;
}

static inline INT32 _stp_btm_notify_assert_timeout_wq(MTKSTP_BTM_T *stp_btm)
{
	INT32 retval;

	if (!stp_btm)
		return STP_BTM_OPERATION_FAIL;

	retval = _stp_btm_dump_type(stp_btm, STP_OPID_BTM_ASSERT_TIMEOUT);
	return retval;
}

static inline INT32 _stp_btm_notify_wmt_dmp_wq(MTKSTP_BTM_T *stp_btm)
{

	INT32 retval;

	if (stp_btm == NULL)
		return STP_BTM_OPERATION_FAIL;

	/* Paged dump */
	retval = _stp_btm_dump_type(stp_btm, STP_OPID_BTM_DBG_DUMP);

	return retval;
}

static inline INT32 _stp_btm_notify_emi_dump_end_wq(MTKSTP_BTM_T *stp_btm)
{
	INT32 retval;

	if (!stp_btm)
		return STP_BTM_OPERATION_FAIL;

	retval = _stp_btm_dump_type(stp_btm, STP_OPID_BTM_EMI_DUMP_END);
	return retval;
}

INT32 stp_btm_notify_wmt_rst_wq(MTKSTP_BTM_T *stp_btm)
{
	return _stp_btm_notify_wmt_rst_wq(stp_btm);
}

INT32 stp_btm_notify_stp_retry_wq(MTKSTP_BTM_T *stp_btm)
{
	return _stp_btm_notify_stp_retry_wq(stp_btm);
}

INT32 stp_btm_notify_coredump_timeout_wq(MTKSTP_BTM_T *stp_btm)
{
	return _stp_btm_notify_coredump_timeout_wq(stp_btm);
}

INT32 stp_btm_notify_assert_timeout_wq(MTKSTP_BTM_T *stp_btm)
{
	return _stp_btm_notify_assert_timeout_wq(stp_btm);
}

INT32 stp_btm_notify_wmt_dmp_wq(MTKSTP_BTM_T *stp_btm)
{
	return _stp_btm_notify_wmt_dmp_wq(stp_btm);
}

INT32 stp_btm_notify_emi_dump_end(MTKSTP_BTM_T *stp_btm)
{
	return _stp_btm_notify_emi_dump_end_wq(stp_btm);
}

INT32 stp_notify_btm_poll_cpupcr_ctrl(UINT32 en)
{
	return stp_dbg_poll_cpupcr_ctrl(en);
}


#if CFG_WMT_LTE_COEX_HANDLING

static inline INT32 _stp_notify_btm_handle_wmt_lte_coex(MTKSTP_BTM_T *stp_btm)
{
	INT32 retval;

	if (stp_btm == NULL)
		return STP_BTM_OPERATION_FAIL;

	retval = _stp_btm_dump_type(stp_btm, STP_OPID_BTM_WMT_LTE_COEX);
	return retval;
}

INT32 stp_notify_btm_handle_wmt_lte_coex(MTKSTP_BTM_T *stp_btm)
{
	return _stp_notify_btm_handle_wmt_lte_coex(stp_btm);
}

#endif
MTKSTP_BTM_T *stp_btm_init(VOID)
{
	INT32 i = 0x0;
	INT32 ret = -1;

	osal_unsleepable_lock_init(&stp_btm->wq_spinlock);
	osal_event_init(&stp_btm->STPd_event);
	stp_btm->wmt_notify = wmt_lib_btm_cb;

	RB_INIT(&stp_btm->rFreeOpQ, STP_BTM_OP_BUF_SIZE);
	RB_INIT(&stp_btm->rActiveOpQ, STP_BTM_OP_BUF_SIZE);

	/* Put all to free Q */
	for (i = 0; i < STP_BTM_OP_BUF_SIZE; i++) {
		osal_signal_init(&(stp_btm->arQue[i].signal));
		_stp_btm_put_op(stp_btm, &stp_btm->rFreeOpQ, &(stp_btm->arQue[i]));
	}

	stp_btm_init_trigger_assert_timer(stp_btm);
	osal_op_history_init(&stp_btm->op_history, 16);

	/*Generate PSM thread, to servie STP-CORE for packet retrying and core dump receiving */
	stp_btm->BTMd.pThreadData = (PVOID) stp_btm;
	stp_btm->BTMd.pThreadFunc = (PVOID) _stp_btm_proc;
	osal_memcpy(stp_btm->BTMd.threadName, BTM_THREAD_NAME, osal_strlen(BTM_THREAD_NAME));

	ret = osal_thread_create(&stp_btm->BTMd);
	if (ret < 0) {
		STP_BTM_PR_ERR("osal_thread_create fail...\n");
		goto ERR_EXIT1;
	}

	/* Start STPd thread */
	ret = osal_thread_run(&stp_btm->BTMd);
	if (ret < 0) {
		STP_BTM_PR_ERR("osal_thread_run FAILS\n");
		goto ERR_EXIT1;
	}

	return stp_btm;

ERR_EXIT1:

	return NULL;

}

INT32 stp_btm_deinit(MTKSTP_BTM_T *stp_btm)
{

	INT32 ret = -1;

	STP_BTM_PR_INFO("btm deinit\n");

	if (!stp_btm)
		return STP_BTM_OPERATION_FAIL;

	ret = osal_thread_destroy(&stp_btm->BTMd);
	if (ret < 0) {
		STP_BTM_PR_ERR("osal_thread_destroy FAILS\n");
		return STP_BTM_OPERATION_FAIL;
	}

	return STP_BTM_OPERATION_SUCCESS;
}


INT32 stp_btm_reset_btm_wq(MTKSTP_BTM_T *stp_btm)
{
	UINT32 i = 0;

	osal_lock_unsleepable_lock(&(stp_btm->wq_spinlock));
	RB_INIT(&stp_btm->rFreeOpQ, STP_BTM_OP_BUF_SIZE);
	RB_INIT(&stp_btm->rActiveOpQ, STP_BTM_OP_BUF_SIZE);
	osal_unlock_unsleepable_lock(&(stp_btm->wq_spinlock));
	/* Put all to free Q */
	for (i = 0; i < STP_BTM_OP_BUF_SIZE; i++) {
		osal_signal_init(&(stp_btm->arQue[i].signal));
		_stp_btm_put_op(stp_btm, &stp_btm->rFreeOpQ, &(stp_btm->arQue[i]));
	}

	return 0;
}


INT32 stp_notify_btm_dump(MTKSTP_BTM_T *stp_btm)
{
	/* pr_warn("%s:enter++\n",__func__); */
	if (stp_btm == NULL) {
		osal_dbg_print("%s: NULL POINTER\n", __func__);
		return -1;
	}
	stp_btm->gDumplogflag = 1;
	osal_trigger_event(&stp_btm->STPd_event);
	return 0;
}

static inline INT32 _stp_btm_do_fw_assert(MTKSTP_BTM_T *stp_btm)
{
	INT32 status = -1;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;

	/* send assert command */
	STP_BTM_PR_INFO("trigger stp assert process\n");
	if (mtk_wcn_stp_is_sdio_mode()) {
		bRet = stp_btm->wmt_notify(BTM_TRIGGER_STP_ASSERT_OP);
		if (bRet == MTK_WCN_BOOL_FALSE) {
			STP_BTM_PR_INFO("trigger stp assert failed\n");
			return status;
		}
		status = 0;
	} else if (mtk_wcn_stp_is_btif_fullset_mode()) {
#if BTIF_RXD_BE_BLOCKED_DETECT
		stp_dbg_is_btif_rxd_be_blocked();
#endif
		status = wmt_plat_force_trigger_assert(STP_FORCE_TRG_ASSERT_DEBUG_PIN);
	}

	stp_btm_start_trigger_assert_timer(stp_btm);

	if (status == 0)
		STP_BTM_PR_INFO("trigger stp assert succeed\n");

	return status;
}


INT32 stp_notify_btm_do_fw_assert(MTKSTP_BTM_T *stp_btm)
{
	return _stp_btm_do_fw_assert(stp_btm);
}

INT32 wmt_btm_trigger_reset(VOID)
{
	return stp_btm_notify_wmt_rst_wq(stp_btm);
}

INT32 stp_btm_set_current_op(MTKSTP_BTM_T *stp_btm, P_OSAL_OP pOp)
{
		if (stp_btm) {
			stp_btm->pCurOP = pOp;
			STP_BTM_PR_DBG("pOp=0x%p\n", pOp);
			return 0;
		}
		STP_BTM_PR_ERR("Invalid pointer\n");
		return -1;
}

P_OSAL_OP stp_btm_get_current_op(MTKSTP_BTM_T *stp_btm)
{
	if (stp_btm)
		return stp_btm->pCurOP;
	STP_BTM_PR_ERR("Invalid pointer\n");
	return NULL;
}

INT32 stp_btm_init_trigger_assert_timer(MTKSTP_BTM_T *stp_btm)
{
	stp_btm->trigger_assert_timer.timeoutHandler = stp_btm_trigger_assert_timeout_handler;
	stp_btm->trigger_assert_timer.timeroutHandlerData = (ULONG)stp_btm;
	stp_btm->timeout = 1000;

	return osal_timer_create(&stp_btm->trigger_assert_timer);
}

INT32 stp_btm_start_trigger_assert_timer(MTKSTP_BTM_T *stp_btm)
{
	return osal_timer_start(&stp_btm->trigger_assert_timer, stp_btm->timeout);
}

INT32 stp_btm_stop_trigger_assert_timer(MTKSTP_BTM_T *stp_btm)
{
	return osal_timer_stop(&stp_btm->trigger_assert_timer);
}

VOID stp_btm_print_op_history(VOID)
{
	osal_op_history_print(&stp_btm->op_history, "_stp_btm_proc");
}
