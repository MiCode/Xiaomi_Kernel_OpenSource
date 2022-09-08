// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/notifier.h>

#include "mtk_qos_bound.h"
#include "mtk_qos_ipi.h"
#include "mtk_qos_common.h"

#include <sspm_define.h>
#include <sspm_reservedmem.h>
#include <sspm_reservedmem_define.h>

#if IS_ENABLED(CONFIG_INTERCONNECT_MTK_MMQOS_COMMON)
#include <soc/mediatek/mmqos.h>
#endif /* CONFIG_INTERCONNECT_MTK_MMQOS_COMMON */

#if IS_ENABLED(CONFIG_MTK_DRAMC)
#include <soc/mediatek/dramc.h>
#endif /* CONFIG_MTK_DRAMC */

#if IS_ENABLED(CONFIG_MTK_EMI)
#include <soc/mediatek/emi.h>
#endif /* CONFIG_MTK_EMI */

static int qos_bound_enabled;
static int qos_bound_stress_enabled;
static int qos_bound_log_enabled;
static unsigned int qos_bound_count;
static unsigned int qos_bound_buf[3];
static BLOCKING_NOTIFIER_HEAD(qos_bound_chain_head);
static struct qos_bound *bound;
static unsigned short *qos_bound_apu;
static unsigned int  qos_bound_apu_num;

int is_qos_bound_enabled(void)
{
	return qos_bound_enabled;
}

void qos_bound_enable(int enable)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2)
	struct qos_ipi_data qos_ipi_d;

	if (!is_mtk_qos_enable())
		return;

	qos_ipi_d.cmd = QOS_IPI_QOS_BOUND_ENABLE;
	qos_ipi_d.u.qos_bound_enable.enable = enable;
	bound = (struct qos_bound *)
			sspm_sbuf_get(qos_ipi_to_sspm_command(&qos_ipi_d, 2));

#elif defined(MTK_SCMI)
	struct qos_ipi_data qos_ipi_d;
	int ack;

	if (!is_mtk_qos_enable())
		return;

	qos_ipi_d.cmd = QOS_IPI_QOS_BOUND_ENABLE;
	qos_ipi_d.u.qos_bound_enable.enable = enable;
	ack = qos_ipi_to_sspm_scmi_command(qos_ipi_d.cmd,
			qos_ipi_d.u.qos_bound_enable.enable, 0, 0, QOS_IPI_SCMI_GET);
	if (!ack || ack == -1) {
		pr_info("get qos sspm address fail\n");
		return;
	}
	bound = (struct qos_bound *)sspm_sbuf_get(ack);
#endif

	if (bound == NULL) {
		pr_info("mtk_qos: sspm_sbuf_get fail\n");
		return;
	}
	smp_mb(); /* init bound before flag enabled */

	if (bound->ver == QOS_BOUND_VER_TAG) {
		qos_bound_apu = (unsigned short *)(bound);
		qos_bound_apu += (sizeof(struct qos_bound)/sizeof(unsigned short));
		qos_bound_apu_num = bound->apu_num;
		pr_info("mtk_qos: bound ver=0x%x apu_num=%d\n",
				bound->ver, bound->apu_num);
		qos_bound_enabled = enable;
	} else {
		pr_info("mtk_qos: invalid bound version(0x%x, 0x%x)\n",
				bound->ver, bound->apu_num);
		qos_bound_enabled = false;
	}
}

int is_qos_bound_stress_enabled(void)
{
	return qos_bound_stress_enabled;
}

void qos_bound_stress_enable(int enable)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2)
	struct qos_ipi_data qos_ipi_d;

	if (!is_mtk_qos_enable())
		return;

	qos_ipi_d.cmd = QOS_IPI_QOS_BOUND_STRESS_ENABLE;
	qos_ipi_d.u.qos_bound_stress_enable.enable = enable;
	qos_ipi_to_sspm_command(&qos_ipi_d, 2);
#elif defined(MTK_SCMI)
	struct qos_ipi_data qos_ipi_d;

	if (!is_mtk_qos_enable())
		return;

	qos_ipi_d.cmd = QOS_IPI_QOS_BOUND_STRESS_ENABLE;
	qos_ipi_d.u.qos_bound_stress_enable.enable = enable;
	//qos_ipi_to_sspm_command(&qos_ipi_d, 2);
	qos_ipi_to_sspm_scmi_command(qos_ipi_d.cmd,
			qos_ipi_d.u.qos_bound_stress_enable.enable, 0, 0, 0);

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
EXPORT_SYMBOL_GPL(get_qos_bound_count);

unsigned int *get_qos_bound_buf(void)
{
	return qos_bound_buf;
}
EXPORT_SYMBOL_GPL(get_qos_bound_buf);

void qos_bound_init(void)
{
	qos_bound_enable(1);
}

struct qos_bound *get_qos_bound(void)
{
	return bound;
}
EXPORT_SYMBOL_GPL(get_qos_bound);

int get_qos_bound_bw_threshold(int state)
{
	int val = 0;

#if IS_ENABLED(CONFIG_MTK_DRAMC) && IS_ENABLED(CONFIG_MTK_EMI)
	val = mtk_dramc_get_steps_freq(0) * mtk_emicen_get_ch_cnt() * 2;
#endif

	if (state == QOS_BOUND_BW_FULL)
		return val * QOS_BOUND_BW_FULL_PCT / 100;
	else if (state == QOS_BOUND_BW_CONGESTIVE)
		return val * QOS_BOUND_BW_CONGESTIVE_PCT / 100;

	return 0;
}
EXPORT_SYMBOL(get_qos_bound_bw_threshold);

unsigned short get_qos_bound_idx(void)
{
	if (!is_qos_bound_enabled())
		return 0;

	return bound->idx;
}
EXPORT_SYMBOL(get_qos_bound_idx);

int register_qos_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&qos_bound_chain_head, nb);
}
EXPORT_SYMBOL(register_qos_notifier);

int unregister_qos_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&qos_bound_chain_head, nb);
}
EXPORT_SYMBOL(unregister_qos_notifier);

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

#if IS_ENABLED(CONFIG_INTERCONNECT_MTK_MMQOS_COMMON)
	mtk_mmqos_system_qos_update(state);
#endif

	if (is_qos_bound_log_enabled()) {
		idx = bound->idx;
		stat = &bound->stats[bound->idx];
		pr_info("idx: %hu, state: %hu, num: %hu, event: %hu\n",
				idx, state,
				stat->num, stat->event);
		for (i = 0; i < NR_QOS_EMIBM_TYPE; i++)
			pr_info("emibw [%d]: mon: %hu\n", i,
					stat->emibw_mon[i]);
		for (i = 0; i < NR_QOS_SMIBM_TYPE; i++)
			pr_info("smibw [%d]: mon: %hu\n", i,
					stat->smibw_mon[i]);
	}

	ret = blocking_notifier_call_chain(&qos_bound_chain_head, val, v);

	return notifier_to_errno(ret);
}

unsigned short get_qos_bound_apubw_mon(int idx, int master)
{
	unsigned short *bw_val;

	if (!is_qos_bound_enabled())
		return 0;

	if (idx < 0 || idx >= QOS_BOUND_BUF_SIZE)
		idx = bound->idx;

	if (master < 0 || master >= qos_bound_apu_num)
		return 0;

	bw_val = qos_bound_apu + (master + idx*qos_bound_apu_num);

	return *bw_val;
}
EXPORT_SYMBOL_GPL(get_qos_bound_apubw_mon);

unsigned short get_qos_bound_apulat_mon(int idx, int master)
{
	unsigned short *lat_val;

	if (!is_qos_bound_enabled())
		return 0;

	if (idx < 0 || idx >= QOS_BOUND_BUF_SIZE)
		idx = bound->idx;

	if (master < 0 || master >= qos_bound_apu_num)
		return 0;

	lat_val = qos_bound_apu + QOS_BOUND_BUF_SIZE*qos_bound_apu_num;
	lat_val += (master + idx*qos_bound_apu_num);

	return *lat_val;
}
EXPORT_SYMBOL_GPL(get_qos_bound_apulat_mon);


unsigned short get_qos_bound_emibw_mon(int idx, int master)
{
	unsigned short val = 0;

	if (!is_qos_bound_enabled())
		return 0;

	if (idx < 0 || idx >= QOS_BOUND_BUF_SIZE)
		idx = bound->idx;

	if (master < 0 || master >= NR_QOS_EMIBM_TYPE)
		master = 0;

	if (idx >= 0)
		val = bound->stats[idx].emibw_mon[master];

	return val;
}
EXPORT_SYMBOL_GPL(get_qos_bound_emibw_mon);

unsigned short get_qos_bound_smibw_mon(int idx, int master)
{
	unsigned short val = 0;

	if (!is_qos_bound_enabled())
		return 0;

	if (idx < 0 || idx >= QOS_BOUND_BUF_SIZE)
		idx = bound->idx;

	if (master < 0 || master >= NR_QOS_SMIBM_TYPE)
		master = 0;

	if (idx >= 0)
		val = bound->stats[idx].smibw_mon[master];

	return val;
}
EXPORT_SYMBOL_GPL(get_qos_bound_smibw_mon);
