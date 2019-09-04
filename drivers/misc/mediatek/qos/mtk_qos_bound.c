// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/notifier.h>

#include "mtk_qos_bound.h"
#include "mtk_qos_ipi.h"

#include <sspm_define.h>
#include <sspm_reservedmem.h>

__weak int dram_steps_freq(unsigned int step) { return 0; }

static int qos_bound_enabled;
static int qos_bound_stress_enabled;
static int qos_bound_log_enabled;
static unsigned int qos_bound_count;
static unsigned int qos_bound_buf[3];
static BLOCKING_NOTIFIER_HEAD(qos_bound_chain_head);
static struct qos_bound *bound;

int is_qos_bound_enabled(void)
{
	return qos_bound_enabled;
}

void qos_bound_enable(int enable)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_BOUND_ENABLE;
	qos_ipi_d.u.qos_bound_enable.enable = enable;
	bound = (struct qos_bound *)
		sspm_sbuf_get(qos_ipi_to_sspm_command(&qos_ipi_d, 2));
	smp_mb(); /* init bound before flag enabled */
#endif
	qos_bound_enabled = enable;
}

int is_qos_bound_stress_enabled(void)
{
	return qos_bound_stress_enabled;
}

void qos_bound_stress_enable(int enable)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_BOUND_STRESS_ENABLE;
	qos_ipi_d.u.qos_bound_stress_enable.enable = enable;
	qos_ipi_to_sspm_command(&qos_ipi_d, 2);
#endif
	qos_bound_stress_enabled = enable;
}

int is_qos_bound_log_enabled(void)
{
	return qos_bound_log_enabled;
}

void qos_bound_log_enable(int enable)
{
	qos_bound_log_enabled = enable;
}

unsigned int get_qos_bound_count(void)
{
	return qos_bound_count;
}
unsigned int *get_qos_bound_buf(void)
{
	return qos_bound_buf;
}

void qos_bound_init(void)
{
	qos_bound_enable(1);
}

struct qos_bound *get_qos_bound(void)
{
	return bound;
}

int get_qos_bound_bw_threshold(int state)
{
	int val = dram_steps_freq(0) * 4;

	if (state == QOS_BOUND_BW_FULL)
		return val * QOS_BOUND_BW_FULL_PCT / 100;
	else if (state == QOS_BOUND_BW_CONGESTIVE)
		return val * QOS_BOUND_BW_CONGESTIVE_PCT / 100;

	return 0;
}

unsigned short get_qos_bound_idx(void)
{
	if (!is_qos_bound_enabled())
		return 0;

	return bound->idx;
}

int register_qos_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&qos_bound_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_qos_notifier);

int unregister_qos_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&qos_bound_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_qos_notifier);

int qos_notifier_call_chain(unsigned long val, void *v)
{
	int ret = NOTIFY_DONE;
	struct qos_bound *bound;
	struct qos_bound_stat *stat;
	unsigned short idx, state;
	int i;

	if (!is_qos_bound_enabled())
		return ret;

	if (v == NULL) {
		pr_info("detect bound null ptr(%d)\n",
			is_qos_bound_enabled());
		return ret;
	}

	bound = (struct qos_bound *) v;
	state = bound->state;
	if (state > 0 && state <= 4) {
		for (i = 0; (state & (1 << i)) == 0; i++)
			;
		qos_bound_count++;
		qos_bound_buf[i]++;
	}

	if (is_qos_bound_log_enabled()) {
		idx = bound->idx;
		stat = &bound->stats[bound->idx];
		pr_info("idx: %hu, state: %hu, num: %hu, event: %hu\n",
				idx, state,
				stat->num, stat->event);
		for (i = 0; i < NR_QOS_EMIBM_TYPE; i++)
			pr_info("emibw [%d]: mon: %hu, req: %hu\n", i,
					stat->emibw_mon[i],
					stat->emibw_req[i]);
		for (i = 0; i < NR_QOS_SMIBM_TYPE; i++)
			pr_info("smibw [%d]: mon: %hu, req: %hu\n", i,
					stat->smibw_mon[i],
					stat->smibw_req[i]);
		for (i = 0; i < NR_QOS_LAT_TYPE; i++)
			pr_info("lat [%d]: mon: %hu\n", i, stat->lat_mon[i]);
	}

	ret = blocking_notifier_call_chain(&qos_bound_chain_head, val, v);

	return notifier_to_errno(ret);
}

