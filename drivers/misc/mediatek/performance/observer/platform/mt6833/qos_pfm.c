/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#define pr_fmt(fmt) "pob_qos: " fmt
#include <linux/notifier.h>
#include <mt-plat/mtk_perfobserver.h>

#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/preempt.h>
#include <linux/proc_fs.h>
#include <linux/trace_events.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/ktime.h>

#include "pob_int.h"
#include "pob_qos.h"
#include "pob_pfm.h"

#ifdef CONFIG_MTK_QOS_FRAMEWORK
#include "mtk_qos_bound.h"
#endif

#define MS_TO_NS 1000000
#define ADJUST_INTERVAL_MS 32

enum POBQOS_NTF_PUSH_TYPE {
	POBQOS_NTF_TIMER = 0x00,
};

struct POBQOS_NTF_PUSH_TAG {
	enum POBQOS_NTF_PUSH_TYPE ePushType;

	int value;

	struct work_struct sWork;
};

#ifdef CONFIG_MTK_QOS_FRAMEWORK
static void pob_enable_timer(void);
static void pob_disable_timer(void);

static atomic64_t last_time_ms;
static atomic_t last_idx;

static int pob_pfm_qos_cb(struct notifier_block *nb,
			unsigned long val, void *data);
static struct notifier_block pob_pfm_qos_notifier = {
	.notifier_call = pob_pfm_qos_cb,
};

static void pob_qos_set_last_time_ms(int setinvalid)
{
	int vIdx;
	ktime_t cur_time;
	long long cur_time_ms;
	struct qos_bound *pqosbound = get_qos_bound();

	if (setinvalid) {
		atomic64_set(&last_time_ms, -1);
		atomic_set(&last_idx, -1);
		return;
	}

	/*get current time*/
	cur_time = ktime_get();
	cur_time_ms = ktime_to_ms(cur_time);
	atomic64_set(&last_time_ms, cur_time_ms);

	if (pqosbound) {
		do {
			vIdx = pqosbound->idx;
		} while (vIdx >= QOS_BOUND_BUF_SIZE || vIdx < 0);

		atomic_set(&last_idx, vIdx);
	}
}

struct qos_bound_stat *_gstats[QOS_BOUND_BUF_SIZE];

static struct workqueue_struct *_gpPOBQoSNtfWQ;
static struct hrtimer _pobqos_hrt;

static void pob_enable_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, ADJUST_INTERVAL_MS * MS_TO_NS);
	hrtimer_start(&_pobqos_hrt, ktime, HRTIMER_MODE_REL);
}

static void pob_disable_timer(void)
{
	hrtimer_cancel(&_pobqos_hrt);
}

static void pobqos_hrt_wq_cb(struct work_struct *psWork)
{
	struct POBQOS_NTF_PUSH_TAG *vpPush =
		POB_CONTAINER_OF(psWork,
				struct POBQOS_NTF_PUSH_TAG, sWork);

	struct qos_bound *pqosbound = get_qos_bound();
	struct pob_qos_info pqi;
	int vIdx;
	int vLastIdx;
	int vTmp = 0;
	int i = 0;
	int size = 0;

	ktime_t cur_time;
	long long cur_ms;
	long long diff_ms;
	long long last_ms;

	if (unlikely(!psWork))
		return;

	/*get current time*/
	cur_time = ktime_get();
	cur_ms = ktime_to_ms(cur_time);

	last_ms = atomic64_read(&last_time_ms);
	vLastIdx = atomic_read(&last_idx);

	if (last_ms == -1 || last_ms >= cur_ms || !pqosbound)
		goto final;

	if (vLastIdx == -1) {
		do {
			vLastIdx = pqosbound->idx;
		} while (vLastIdx >= QOS_BOUND_BUF_SIZE ||
				vLastIdx < 0);

		atomic_set(&last_idx, vLastIdx);
		goto final;
	}

	diff_ms = cur_ms - last_ms;

	if (diff_ms < 2)
		goto final;
	else if (diff_ms > QOS_BOUND_BUF_SIZE - 2)
		diff_ms = QOS_BOUND_BUF_SIZE - 2;

	do {
		vIdx = pqosbound->idx;
	} while (vIdx >= QOS_BOUND_BUF_SIZE);

	if (diff_ms == QOS_BOUND_BUF_SIZE - 2)
		vLastIdx = (vIdx + 3) % QOS_BOUND_BUF_SIZE;

	if (vLastIdx > vIdx) {
		for (i = 0; vLastIdx + i < QOS_BOUND_BUF_SIZE; i++) {

			_gstats[i] = &pqosbound->stats[vLastIdx + i];
			size++;
		}

		vLastIdx = 0;
	}

	for (vTmp = size, i = 0; vLastIdx + i < vIdx; i++) {
		_gstats[vTmp + i] = &pqosbound->stats[vLastIdx + i];
		size++;
	}

	atomic64_set(&last_time_ms, cur_ms);
	atomic_set(&last_idx, vIdx);

	if (size) {
		pqi.size = size;
		pqi.pstats = &_gstats;

		pob_qos_tracelog(POB_QOS_EMI_ALL, &pqi);
		pob_qos_monitor_update(POB_QOS_EMI_ALL, &pqi);
	}

final:
	pob_trace("%s size=%d", __func__, size);

	pob_free(vpPush);
}

static enum hrtimer_restart pobqos_hrt_cb(struct hrtimer *timer)
{
	struct POBQOS_NTF_PUSH_TAG *vpPush = NULL;

	ktime_t ktime;

	ktime = ktime_set(0, ADJUST_INTERVAL_MS * MS_TO_NS);
	hrtimer_add_expires(timer, ktime);

	if (_gpPOBQoSNtfWQ)
		vpPush =
			(struct POBQOS_NTF_PUSH_TAG *)
			pob_alloc_atomic(sizeof(struct POBQOS_NTF_PUSH_TAG));

	if (vpPush) {
		INIT_WORK(&vpPush->sWork, pobqos_hrt_wq_cb);
		queue_work(_gpPOBQoSNtfWQ, &vpPush->sWork);
	}

	return HRTIMER_RESTART;
}

static int pob_pfm_qos_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct qos_bound *bound = (struct qos_bound *) data;
	struct qos_bound_stat *pstats[ADJUST_INTERVAL_MS];

	switch (val) {
	case QOS_BOUND_BW_FULL:
	case QOS_BOUND_BW_CONGESTIVE:
	case QOS_BOUND_BW_FREE:
		if (!pob_qos_ind_client_isemtpy() && bound) {
			struct pob_qos_info pqi;
			int vLastIdx;
			int vTmp = 0;
			int i;
			int ind_info;

			do {
				vLastIdx = bound->idx;
			} while (vLastIdx >= QOS_BOUND_BUF_SIZE);

			vTmp = ADJUST_INTERVAL_MS;

			do {
				i = vLastIdx + QOS_BOUND_BUF_SIZE + 1;
				i -= vTmp;
				i %= QOS_BOUND_BUF_SIZE;

				pstats[ADJUST_INTERVAL_MS - vTmp] =
					&bound->stats[i];

				vTmp--;
			} while (vTmp > 0);

			pqi.size = ADJUST_INTERVAL_MS;
			pqi.pstats = &pstats;

			ind_info = POB_QOS_IND_BWBOUND_CONGESTIVE;
			if (val == QOS_BOUND_BW_FREE)
				ind_info = POB_QOS_IND_BWBOUND_FREE;
			else if (val == QOS_BOUND_BW_FULL)
				ind_info = POB_QOS_IND_BWBOUND_FULL;

			pob_qos_ind_monitor_update(ind_info, &pqi);
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

int __init pob_qos_pfm_init(void)
{
	_gpPOBQoSNtfWQ = create_singlethread_workqueue("pobqos_ntf_wq");
	if (_gpPOBQoSNtfWQ == NULL)
		return -EFAULT;

	hrtimer_init(&_pobqos_hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	_pobqos_hrt.function = pobqos_hrt_cb;

	register_qos_notifier(&pob_pfm_qos_notifier);

	return 0;
}

void __exit pob_qos_pfm_exit(void)
{
	unregister_qos_notifier(&pob_pfm_qos_notifier);
}

int pob_qos_pfm_enable(void)
{
	pob_qos_set_last_time_ms(0);
	pob_enable_timer();

	return 0;
}

int pob_qos_pfm_disable(void)
{
	pob_disable_timer();
	pob_qos_set_last_time_ms(1);

	return 0;
}

int pob_qosbm_get_cap(enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int *super,
			int *sub)
{
	if (!super || !sub)
		return -1;

	*super = 0;
	*sub = 0;

	switch (pqbt) {

	case PQBT_BOUND:
	case PQBT_TOTAL:
		if (pqbp == PQBP_EMI &&
			pqbs == PQBS_MON)
			*super = 1;
		break;
	case PQBT_CPU:
	case PQBT_MM:
	case PQBT_MD:
		if (pqbp == PQBP_EMI &&
			(pqbs == PQBS_MON ||
			 pqbs == PQBS_REQ))
			*super = 1;
		break;
	case PQBT_GPU:
		if ((pqbp == PQBP_EMI || pqbp == PQBP_SMI) &&
			(pqbs == PQBS_MON || pqbs == PQBS_REQ))
			*super = 1;
		break;
	case PQBT_APU:
	case PQBT_MDLA:
	case PQBT_VENC:
	case PQBT_CAM:
	case PQBT_IMG:
	case PQBT_MDP:
		if ((pqbp == PQBP_SMI) &&
			(pqbs == PQBS_MON || pqbs == PQBS_REQ))
			*super = 1;
		break;
	case PQBT_VPU:
		if ((pqbp == PQBP_SMI) &&
			(pqbs == PQBS_MON || pqbs == PQBS_REQ))
			*sub = 2;
		break;

	default:
		return -1;
	}

	if (*super == 0 && *sub == 0)
		return -1;

	return 0;
}

int pob_qosbm_get_stat(void *pstats,
			int idx,
			enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int issub,
			int ssidx)
{
	int super = 0, sub = 0;
	int vRet = -1;

	struct qos_bound_stat **bound =
		(struct qos_bound_stat **) pstats;

	if (!bound || !bound[idx])
		return -1;

	vRet = pob_qosbm_get_cap(pqbt, pqbp, pqbs, &super, &sub);

	if (vRet)
		return -1;
	else if (issub && ssidx >= sub)
		return -1;
	else if (!issub && ssidx >= super)
		return -1;

	switch (pqbt) {
	case PQBT_BOUND:
		if (pqbp == PQBP_EMI && pqbs == PQBS_MON) {
			vRet = PQB_BW_CONGESTIVE;
			if (bound[idx]->event == QOS_BOUND_BW_FREE)
				vRet = PQB_BW_FREE;
			else if (bound[idx]->event == QOS_BOUND_BW_FULL)
				vRet = PQB_BW_FULL;
		}
		break;
	case PQBT_TOTAL:
		if (pqbp == PQBP_EMI && pqbs == PQBS_MON)
			vRet = bound[idx]->emibw_mon[QOS_EMIBM_TOTAL];
		else if (pqbp == PQBP_EMI && pqbs == PQBS_REQ)
			vRet = bound[idx]->emibw_req[QOS_EMIBM_TOTAL];
		break;
	case PQBT_CPU:
		if (pqbp == PQBP_EMI && pqbs == PQBS_MON)
			vRet = bound[idx]->emibw_mon[QOS_EMIBM_CPU];
		else if (pqbp == PQBP_EMI && pqbs == PQBS_REQ)
			vRet = bound[idx]->emibw_req[QOS_EMIBM_CPU];
		break;
	case PQBT_GPU:
		if (pqbp == PQBP_EMI) {
			if (pqbs == PQBS_MON)
				vRet = bound[idx]->emibw_mon[QOS_EMIBM_GPU];
			else if (pqbs == PQBS_REQ)
				vRet = bound[idx]->emibw_req[QOS_EMIBM_GPU];
		} else if (pqbp == PQBP_SMI) {
			if (pqbs == PQBS_MON)
				vRet = bound[idx]->smibw_mon[QOS_SMIBM_GPU];
			else if (pqbs == PQBS_REQ)
				vRet = bound[idx]->smibw_req[QOS_SMIBM_GPU];
		}
		break;
	case PQBT_MM:
		if (pqbp == PQBP_EMI) {
			if (pqbs == PQBS_MON)
				vRet = bound[idx]->emibw_mon[QOS_EMIBM_MM];
			else if (pqbs == PQBS_REQ)
				vRet = bound[idx]->emibw_req[QOS_EMIBM_MM];
		}
		break;
	case PQBT_MD:
		if (pqbp == PQBP_EMI) {
			if (pqbs == PQBS_MON)
				vRet = bound[idx]->emibw_mon[QOS_EMIBM_MD];
			else if (pqbs == PQBS_REQ)
				vRet = bound[idx]->emibw_req[QOS_EMIBM_MD];
		}
		break;
	case PQBT_APU:
		if (pqbp == PQBP_SMI) {
			if (pqbs == PQBS_MON)
				vRet = bound[idx]->smibw_mon[QOS_SMIBM_APU];
			else if (pqbs == PQBS_REQ)
				vRet = bound[idx]->smibw_req[QOS_SMIBM_APU];
		}
		break;
	case PQBT_VPU:
		if (pqbp == PQBP_SMI) {
			int tg;

			if (ssidx == 1)
				tg = QOS_SMIBM_VPU1;
			else
				tg = QOS_SMIBM_VPU0;

			if (pqbs == PQBS_MON)
				vRet = bound[idx]->smibw_mon[tg];
			else if (pqbs == PQBS_REQ)
				vRet = bound[idx]->smibw_req[tg];
		}
		break;
	case PQBT_VENC:
		if (pqbp == PQBP_SMI) {
			if (pqbs == PQBS_MON)
				vRet = bound[idx]->smibw_mon[QOS_SMIBM_VENC];
			else if (pqbs == PQBS_REQ)
				vRet = bound[idx]->smibw_req[QOS_SMIBM_VENC];
		}
		break;
	case PQBT_CAM:
		if (pqbp == PQBP_SMI) {
			if (pqbs == PQBS_MON)
				vRet = bound[idx]->smibw_mon[QOS_SMIBM_CAM];
			else if (pqbs == PQBS_REQ)
				vRet = bound[idx]->smibw_req[QOS_SMIBM_CAM];
		}
		break;
	case PQBT_IMG:
		if (pqbp == PQBP_SMI) {
			if (pqbs == PQBS_MON)
				vRet = bound[idx]->smibw_mon[QOS_SMIBM_IMG];
			else if (pqbs == PQBS_REQ)
				vRet = bound[idx]->smibw_req[QOS_SMIBM_IMG];
		}
		break;
	case PQBT_MDP:
		if (pqbp == PQBP_SMI) {
			if (pqbs == PQBS_MON)
				vRet = bound[idx]->smibw_mon[QOS_SMIBM_MDP];
			else if (pqbs == PQBS_REQ)
				vRet = bound[idx]->smibw_req[QOS_SMIBM_MDP];
		}
		break;
	default:
		return -1;
	}

	return vRet;
}

int pob_qoslat_get_cap(enum pob_qoslat_type pqlt,
			enum pob_qoslat_source pqls,
			int *super,
			int *sub)
{
	if (!super || !sub)
		return -1;

	*super = 0;
	*sub = 0;

	switch (pqlt) {
	case PQLT_CPU:
	case PQLT_MDLA:
		if (pqls == PQLS_MON)
			*super = 1;
		break;
	case PQLT_VPU:
		if (pqls == PQLS_MON)
			*sub = 2;
		break;
	default:
		return -1;
	}

	if (*super == 0 && *sub == 0)
		return -1;

	return 0;
}

int pob_qoslat_get_stat(void *pstats,
			int idx,
			enum pob_qoslat_type pqlt,
			enum pob_qoslat_source pqls,
			int issub,
			int ssidx)
{
	int super = 0, sub = 0;
	int vRet = -1;
	int tg;

	struct qos_bound_stat **bound =
		(struct qos_bound_stat **) pstats;

	if (!bound || !bound[idx])
		return -1;

	vRet = pob_qoslat_get_cap(pqlt, pqls, &super, &sub);

	if (vRet)
		return -1;
	else if (issub && ssidx >= sub)
		return -1;
	else if (!issub && ssidx >= super)
		return -1;

	switch (pqlt) {
	case PQLT_CPU:
		if (pqls == PQLS_MON)
			vRet = bound[idx]->lat_mon[QOS_LAT_CPU];
		break;
	case PQLT_VPU:
		if (ssidx == 1)
			tg = QOS_LAT_VPU1;
		else
			tg = QOS_LAT_VPU0;

		if (pqls == PQLS_MON)
			vRet = bound[idx]->lat_mon[tg];
		break;
	default:
		return -1;
	}

	return vRet;
}

int pob_qosseq_get(void *pstats, int idx)
{
	struct qos_bound_stat **bound =
		(struct qos_bound_stat **) pstats;

	if (bound && bound[idx])
		return bound[idx]->num;

	return -1;
}

int pob_qos_get_max_bw_threshold(void)
{
	return get_qos_bound_bw_threshold(QOS_BOUND_BW_FULL);
}

int pob_qosbm_get_last_avg(int lastcount,
			enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int issub,
			int ssidx)
{
	int vRet;
	int super = 0, sub = 0;
	struct qos_bound *bound = get_qos_bound();

	int vLastIdx;
	int vTmp = 0;
	int vTotal = 0;
	int i = 0;

	vRet = pob_qosbm_get_cap(pqbt, pqbp, pqbs, &super, &sub);

	if (vRet)
		return -1;
	else if (issub && ssidx >= sub)
		return -1;
	else if (!issub && ssidx >= super)
		return -1;

	if (!bound)
		return 0;

	do {
		vLastIdx = bound->idx;
	} while (vLastIdx >= QOS_BOUND_BUF_SIZE ||
				vLastIdx < 0);

	if (lastcount >= QOS_BOUND_BUF_SIZE)
		lastcount = QOS_BOUND_BUF_SIZE - 1;
	else if (lastcount <= 0)
		lastcount = QOS_BOUND_BUF_SIZE >> 1;

	vTmp = lastcount;
	vRet = -1;

	switch (pqbt) {
	case PQBT_TOTAL:
		if (pqbp == PQBP_EMI && pqbs == PQBS_MON) {
			do {
				i = vLastIdx + QOS_BOUND_BUF_SIZE + 1;
				i -= vTmp;
				i %= QOS_BOUND_BUF_SIZE;

				vTotal +=
				bound->stats[i].emibw_mon[QOS_EMIBM_TOTAL];

				vTmp--;
			} while (vTmp > 0);

			vRet = vTotal;
			do_div(vRet, lastcount);
		}
		break;
	default:
		break;
	}

	return vRet;
}

#else
int pob_qos_pfm_enable(void)
{
	return 0;
}

int pob_qos_pfm_disable(void)
{
	return 0;
}

int pob_qosseq_get(void *pstats, int idx)
{
	return -1;
}

int pob_qosbm_get_cap(enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int *super,
			int *sub)
{
	return -1;
}

int pob_qosbm_get_stat(void *pstats,
			int idx,
			enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int issub,
			int ssidx)
{
	return -1;
}

int pob_qos_get_max_bw_threshold(void)
{
	return -1;
}

int pob_qosbm_get_last_avg(int lastcount,
			enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int issub,
			int ssidx)
{
	return -1;
}

int pob_qoslat_get_cap(enum pob_qoslat_type pqlt,
			enum pob_qoslat_source pqls,
			int *super,
			int *sub)
{
	return -1;
}

int pob_qoslat_get_stat(void *pstats,
			int idx,
			enum pob_qoslat_type pqlt,
			enum pob_qoslat_source pqls,
			int issub,
			int ssidx)
{
	return -1;
}

int __init pob_qos_pfm_init(void)
{
	return 0;
}

void __exit pob_qos_pfm_exit(void)
{
}
#endif

void pob_qos_tracker(u64 wallclock)
{
}

