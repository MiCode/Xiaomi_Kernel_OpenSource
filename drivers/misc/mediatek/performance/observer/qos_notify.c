// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
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
#include <linux/trace_events.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/ktime.h>

#include <trace/events/pob.h>

#include "pob_int.h"
#include "pob_qos.h"
#include "pob_pfm.h"

#define TRACELOG_SIZE 512

struct pob_qos_trace_info {
	uint32_t flag;
	int typemap;
	char *prefix;
	void (*pHdlfun)(struct pob_qos_trace_info *pqti,
			struct pob_qos_info *pqi);
};

static BLOCKING_NOTIFIER_HEAD(pob_qos_notifier_list);
static BLOCKING_NOTIFIER_HEAD(pob_qos_ind_notifier_list);

static DEFINE_MUTEX(pob_qos_ntf_mutex);
static int pob_qos_ntf_cnt;

static inline void _trace_pob_log(char *log)
{
	preempt_disable();
	trace_pob_log(log);
	preempt_enable();
}

static void pob_fn_tracelog(void *pfn, char *prefix)
{
	char log[TRACELOG_SIZE];
	int tmplen;

	tmplen = snprintf(log, TRACELOG_SIZE, "%s %pS",
				prefix, pfn);
	if (tmplen < 0 || tmplen >= sizeof(log))
		return;

	_trace_pob_log(log);
}

static int __pob_dump_ntf_callchain(struct notifier_block **nl, char *prefix)
{
	int ret = NOTIFY_DONE;
	struct notifier_block *nb, *next_nb;

	nb = rcu_dereference_raw(*nl);

	while (nb) {
		next_nb = rcu_dereference_raw(nb->next);

		pob_fn_tracelog(nb->notifier_call, prefix);

		nb = next_nb;
	}
	return ret;
}

static int pob_dump_ntf_callchain(struct blocking_notifier_head *nh,
					char *prefix)
{
	int ret = NOTIFY_DONE;

	/*
	 * We check the head outside the lock, but if this access is
	 * racy then it does not matter what the result of the test
	 * is, we re-check the list after having taken the lock anyway:
	 */
	if (rcu_access_pointer(nh->head)) {
		down_read(&nh->rwsem);
		ret = __pob_dump_ntf_callchain(&nh->head, prefix);
		up_read(&nh->rwsem);
	}
	return ret;
}

int pob_qos_register_client(struct notifier_block *nb)
{
	mutex_lock(&pob_qos_ntf_mutex);

	if (!pob_qos_ntf_cnt)
		pob_qos_pfm_enable();

	pob_qos_ntf_cnt++;
	mutex_unlock(&pob_qos_ntf_mutex);

	pob_fn_tracelog(nb->notifier_call, "pob_qos register_client");
	pob_dump_ntf_callchain(&pob_qos_notifier_list,
				"pob_qos register_client before");
	return blocking_notifier_chain_register(&pob_qos_notifier_list, nb);
}

int pob_qos_unregister_client(struct notifier_block *nb)
{
	mutex_lock(&pob_qos_ntf_mutex);
	if (pob_qos_ntf_cnt)
		pob_qos_ntf_cnt--;

	if (!pob_qos_ntf_cnt)
		pob_qos_pfm_disable();

	mutex_unlock(&pob_qos_ntf_mutex);

	pob_fn_tracelog(nb->notifier_call, "pob_qos unregister_client");
	pob_dump_ntf_callchain(&pob_qos_notifier_list,
				"pob_qos unregister_client before");
	return blocking_notifier_chain_unregister(&pob_qos_notifier_list, nb);
}

int pob_qos_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&pob_qos_notifier_list, val, v);
}

int pob_qos_monitor_update(enum pob_qos_info_num info_num, void *v)
{
	pob_qos_notifier_call_chain(info_num, v);

	return 0;
}

int pob_qos_ind_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&pob_qos_ind_notifier_list,
							nb);
}

int pob_qos_ind_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&pob_qos_ind_notifier_list,
							nb);
}

int pob_qos_ind_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&pob_qos_ind_notifier_list,
						val, v);
}

int pob_qos_ind_monitor_update(enum pob_qos_ind_info_num info_num, void *v)
{
	pob_qos_ind_notifier_call_chain(info_num, v);

	return 0;
}

int pob_qos_ind_client_isemtpy(void)
{
	if (pob_qos_ind_notifier_list.head)
		return 0;

	return 1;
}

enum {
	POB_QOS_DEBUG_QOSSEQ  = 1U << 0,
	POB_QOS_DEBUG_BWBOUND = 1U << 1,
	POB_QOS_DEBUG_BWTOTAL = 1U << 2,
	POB_QOS_DEBUG_BWCPU   = 1U << 3,
	POB_QOS_DEBUG_BWGPU   = 1U << 4,
	POB_QOS_DEBUG_BWMM    = 1U << 5,
	POB_QOS_DEBUG_BWMD    = 1U << 6,
	POB_QOS_DEBUG_BWAPU   = 1U << 7,
	POB_QOS_DEBUG_BWVPU   = 1U << 8,
	POB_QOS_DEBUG_BWMDLA  = 1U << 9,
	POB_QOS_DEBUG_BWCAM   = 1U << 10,
	POB_QOS_DEBUG_BWVENC  = 1U << 11,
	POB_QOS_DEBUG_BWIMG   = 1U << 12,
	POB_QOS_DEBUG_BWMDP   = 1U << 13,
	POB_QOS_DEBUG_LATCPU  = 1U << 14,
	POB_QOS_DEBUG_LATVPU  = 1U << 15,
	POB_QOS_DEBUG_LATMDLA = 1U << 16,
	POB_QOS_DEBUG_MAX     = 1U << 17,
};


static const char * const mask_string[] = {
	"QOSSEQ",
	"BWBOUND",
	"BWTOTAL",
	"BWCPU",
	"BWGPU",
	"BWMM",
	"BWMD",
	"BWAPU",
	"BWVPU",
	"BWMDLA",
	"BWCAM",
	"BWVENC",
	"BWIMG",
	"BWMDP",
	"LATCPU",
	"LATVPU",
	"LATMDLA",
	"MAX",
};

static const char * const PQBP_str[] = {
	"EMI",
	"SMI"
};

static const char * const PQBS_str[] = {
	"MON",
	"REQ"
};

static const char * const PQLS_str[] = {
	"MON",
};

static void _pob_qos_seq_log(struct pob_qos_trace_info *pqti,
			struct pob_qos_info *pqi)
{
	int i;
	char log[TRACELOG_SIZE];
	char valuelog[8];
	int tmplen;
	int sublen;

	tmplen = snprintf(log, TRACELOG_SIZE, "%s size=%d,",
				pqti->prefix, pqi->size);
	if (tmplen < 0 || tmplen >= sizeof(log))
		return;

	for (i = 0; i < pqi->size; i++) {
		sublen = snprintf(valuelog, 8, " %d",
			pob_qosseq_get(pqi->pstats, i));
		if (sublen >= 8)
			break;
		if (tmplen + sublen >= TRACELOG_SIZE - 1)
			break;
		strncpy(log + tmplen, valuelog,
			TRACELOG_SIZE - 1 - tmplen);
		tmplen += sublen;
	}

	_trace_pob_log(log);
}

static void _pob_qos_bm_log(struct pob_qos_trace_info *pqti,
			struct pob_qos_info *pqi)
{
	int i, j, k;
	int x;
	int super, sub;
	char log[TRACELOG_SIZE];
	char valuelog[8];
	int tmplen;
	int vRet;
	int sublen;

	for (i = 0; i < NR_PQBP; i++) {
		for (j = 0; j < NR_PQBS; j++) {
			vRet = pob_qosbm_get_cap(pqti->typemap, i, j,
							&super, &sub);

			if (vRet)
				continue;

			if (super) {
				tmplen = snprintf(log, TRACELOG_SIZE,
					"%s probe=%s, source=%s, size=%d,",
					pqti->prefix,
					PQBP_str[i], PQBS_str[j], pqi->size);
				if (tmplen < 0 || tmplen >= sizeof(log))
					return;

				for (k = 0; k < pqi->size; k++) {
					sublen = snprintf(valuelog, 8, " %d",
						pob_qosbm_get_stat(pqi->pstats,
							k, pqti->typemap,
							i, j, 0, 0));
					if (sublen >= 8)
						break;
					if (tmplen + sublen >=
						TRACELOG_SIZE - 1)
						break;
					strncpy(log + tmplen, valuelog,
						TRACELOG_SIZE - 1 - tmplen);
					tmplen += sublen;
				}

				_trace_pob_log(log);
			}

			for (x = 0; x < sub; x++) {
				tmplen = snprintf(log, TRACELOG_SIZE,
					"%s%d probe=%s, source=%s, size=%d,",
					pqti->prefix, x,
					PQBP_str[i], PQBS_str[j], pqi->size);
				if (tmplen < 0 || tmplen >= sizeof(log))
					return;

				for (k = 0; k < pqi->size; k++) {
					sublen = snprintf(valuelog, 8, " %d",
						pob_qosbm_get_stat(pqi->pstats,
							k, pqti->typemap,
							i, j, 1, x));
					if (sublen >= 8)
						break;
					if (tmplen + sublen >=
						TRACELOG_SIZE - 1)
						break;
					strncpy(log + tmplen, valuelog,
						TRACELOG_SIZE - 1 - tmplen);
					tmplen += sublen;
				}

				_trace_pob_log(log);
			}
		}
	}
}

static void _pob_qos_lat_log(struct pob_qos_trace_info *pqti,
			struct pob_qos_info *pqi)
{
	int i, k;
	int x;
	int super, sub;
	char log[TRACELOG_SIZE];
	char valuelog[8];
	int tmplen;
	int vRet;
	int sublen;

	for (i = 0; i < NR_PQLS; i++) {
		vRet = pob_qoslat_get_cap(pqti->typemap, i,
					&super, &sub);

		if (vRet)
			continue;

		if (super) {
			tmplen = snprintf(log, TRACELOG_SIZE,
				"%s source=%s, size=%d,",
				pqti->prefix,
				PQLS_str[i], pqi->size);
			if (tmplen < 0 || tmplen >= sizeof(log))
				return;

			for (k = 0; k < pqi->size; k++) {
				sublen = snprintf(valuelog, 8, " %d",
					pob_qoslat_get_stat(pqi->pstats,
						k, pqti->typemap,
						i, 0, 0));
				if (sublen >= 8)
					break;
				if (tmplen + sublen >=
					TRACELOG_SIZE - 1)
					break;
				strncpy(log + tmplen, valuelog,
					TRACELOG_SIZE - 1 - tmplen);
				tmplen += sublen;
			}

			_trace_pob_log(log);
		}

		for (x = 0; x < sub; x++) {
			tmplen = snprintf(log, TRACELOG_SIZE,
				"%s%d source=%s, size=%d,",
				pqti->prefix, x,
				PQLS_str[i], pqi->size);
			if (tmplen < 0 || tmplen >= sizeof(log))
				return;

			for (k = 0; k < pqi->size; k++) {
				sublen = snprintf(valuelog, 8, " %d",
					pob_qoslat_get_stat(pqi->pstats,
						k, pqti->typemap,
						i, 1, x));
				if (sublen >= 8)
					break;
				if (tmplen + sublen >=
					TRACELOG_SIZE - 1)
					break;
				strncpy(log + tmplen, valuelog,
					TRACELOG_SIZE - 1 - tmplen);
				tmplen += sublen;
			}

			_trace_pob_log(log);
		}
	}
}

static struct pob_qos_trace_info pqti[] = {
	{POB_QOS_DEBUG_QOSSEQ, -1, "QOS_SEQ",
	_pob_qos_seq_log},

	{POB_QOS_DEBUG_BWBOUND, PQBT_BOUND, "BW_BOUND",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWTOTAL, PQBT_TOTAL, "BW_TOTAL",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWCPU, PQBT_CPU, "BW_CPU",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWGPU, PQBT_GPU, "BW_GPU",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWMM, PQBT_MM, "BW_MM",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWMD, PQBT_MD, "BW_MD",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWAPU, PQBT_APU, "BW_APU",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWVPU, PQBT_VPU, "BW_VPU",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWMDLA, PQBT_MDLA, "BW_MDLA",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWCAM, PQBT_CAM, "BW_CAM",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWVENC, PQBT_VENC, "BW_VENC",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWIMG, PQBT_IMG, "BW_IMG",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_BWMDP, PQBT_MDP, "BW_MDP",
	_pob_qos_bm_log},

	{POB_QOS_DEBUG_LATCPU, PQLT_CPU, "LAT_CPU",
	_pob_qos_lat_log},

	{POB_QOS_DEBUG_LATVPU, PQLT_VPU, "LAT_VPU",
	_pob_qos_lat_log},

	{POB_QOS_DEBUG_LATMDLA, PQLT_MDLA, "LAT_MDLA",
	_pob_qos_lat_log},

	{POB_QOS_DEBUG_MAX, -1, NULL, 0},
};

static uint32_t pob_qos_systrace_mask;
static uint32_t pob_qos_forceon;

void pob_qos_tracelog(unsigned long val, void *data)
{
	int i;
	struct pob_qos_info *pqi = (struct pob_qos_info *) data;

	switch (val) {
	case POB_QOS_EMI_ALL:
		for (i = 0; pqti[i].flag != POB_QOS_DEBUG_MAX; i++) {
			if (pob_qos_systrace_mask & pqti[i].flag)
				pqti[i].pHdlfun(&pqti[i], pqi);
		}
		break;
	default:
		break;
	}
}

static int _pob_qos_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	return NOTIFY_OK;
}

static struct notifier_block pob_qos_notifier = {
	.notifier_call = _pob_qos_cb,
};

static inline void _pob_qos_enable(void)
{
	pob_qos_register_client(&pob_qos_notifier);
}

static inline void _pob_qos_disable(void)
{
	pob_qos_unregister_client(&pob_qos_notifier);
}

static ssize_t pob_mask_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i;
	char temp[POB_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	length = scnprintf(temp + pos, POB_SYSFS_MAX_BUFF_SIZE - pos,
			" Current enabled systrace:\n");
	pos += length;

	for (i = 0; (1U << i) < POB_QOS_DEBUG_MAX; i++) {
		length = scnprintf(temp + pos, POB_SYSFS_MAX_BUFF_SIZE - pos,
			"  %-*s ... %s\n", 12, mask_string[i],
		   pob_qos_systrace_mask & (1U << i) ?
		   "On" : "Off");
		pos += length;

	}

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t pob_mask_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[POB_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < POB_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, POB_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	val = val & (POB_QOS_DEBUG_MAX - 1U);

	pob_qos_systrace_mask = val;

	return count;
}

KOBJ_ATTR_RW(pob_mask);

static ssize_t pob_forceon_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int val = -1;

	val = pob_qos_forceon;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t pob_forceon_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[POB_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < POB_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, POB_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	if (val > 1 || val < 0)
		return count;

	if (val && !pob_qos_forceon)
		_pob_qos_enable();
	else if (!val && pob_qos_forceon)
		_pob_qos_disable();

	pob_qos_forceon = val;

	return count;
}

KOBJ_ATTR_RW(pob_forceon);

int __init pob_qos_init(struct kobject *pob_kobj)
{
	if (pob_qos_pfm_init())
		return -EFAULT;

	pob_qos_systrace_mask =
		POB_QOS_DEBUG_QOSSEQ |
		POB_QOS_DEBUG_BWBOUND |
		POB_QOS_DEBUG_BWTOTAL;

	if (!pob_kobj)
		return -ENODEV;

	pob_sysfs_create_file(pob_kobj, &kobj_attr_pob_forceon);
	pob_sysfs_create_file(pob_kobj, &kobj_attr_pob_mask);

	return 0;
}

void __exit pob_qos_exit(struct kobject *pob_kobj)
{
	pob_sysfs_remove_file(pob_kobj, &kobj_attr_pob_forceon);
	pob_sysfs_remove_file(pob_kobj, &kobj_attr_pob_mask);

	pob_qos_pfm_exit();
}

