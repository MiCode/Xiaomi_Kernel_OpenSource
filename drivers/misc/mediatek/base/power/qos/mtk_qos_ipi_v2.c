/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/kthread.h>
#include <linux/mutex.h>

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_ipi_id.h>
#include <sspm_define.h>
#endif

#include "mtk_qos_bound.h"
#ifdef QOS_PREFETCH_SUPPORT
#include "mtk_qos_prefetch.h"
#endif /* QOS_PREFETCH_SUPPORT */
#include "mtk_qos_ipi.h"

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
static DEFINE_MUTEX(qos_ipi_mutex);
static int qos_sspm_ready;
int qos_ipi_ackdata;
struct qos_ipi_data qos_recv_ackdata;

static void qos_sspm_enable(void)
{
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_ENABLE;
	qos_ipi_to_sspm_command(&qos_ipi_d, 1);
}

static int qos_ipi_recv_thread(void *arg)
{
	struct qos_ipi_data *qos_ipi_d;

	pr_info("%s start!\n", __func__);
	do {
		mtk_ipi_recv(&sspm_ipidev, IPIR_I_QOS);

		qos_ipi_d = &qos_recv_ackdata;

		switch (qos_ipi_d->cmd) {
		case QOS_IPI_QOS_BOUND:
			qos_notifier_call_chain(
					qos_ipi_d->u.qos_bound.state,
					get_qos_bound());
			break;
#ifdef QOS_PREFETCH_SUPPORT
		case QOS_IPI_QOS_PREFETCH_CB:
			prefetch_notifier_call_chain(
					qos_ipi_d->u.qos_prefetch_cb.state,
					NULL);
			break;
		case QOS_IPI_QOS_PREFETCH_UPDATE:
			if (is_qos_prefetch_enabled())
				qos_prefetch_update_all();
			break;
#endif /* QOS_PREFETCH_SUPPORT */
		default:
			pr_info("wrong QoS IPI command: %d\n", qos_ipi_d->cmd);
		}
	} while (!kthread_should_stop());

	return 0;
}
#endif

int qos_ipi_to_sspm_command(void *buffer, int slot)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	int ret, ackdata;
	struct qos_ipi_data *qos_ipi_d = buffer;
	int slot_num = sizeof(struct qos_ipi_data)/SSPM_MBOX_SLOT_SIZE;

	mutex_lock(&qos_ipi_mutex);

	if (qos_sspm_ready != 1) {
		pr_info("qos ipi not ready, skip cmd=%d\n", qos_ipi_d->cmd);
		goto error;
	}

	qos_ipi_ackdata = 0;

	if (slot > slot_num) {
		pr_info("qos ipi cmd %d req slot error(%d > %d)\n",
			qos_ipi_d->cmd, slot, slot_num);
		goto error;
	}

	ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_QOS,
		IPI_SEND_POLLING, buffer,
		slot_num, 2000);
	if (ret) {
		pr_info("qos ipi cmd %d send fail,ret=%d\n",
		qos_ipi_d->cmd, ret);
		goto error;
	}

	if (!qos_ipi_ackdata) {
		pr_info("qos ipi cmd %d ack fail, ackdata=%d\n",
		qos_ipi_d->cmd, qos_ipi_ackdata);
		goto error;
	}
	ackdata = qos_ipi_ackdata;
	mutex_unlock(&qos_ipi_mutex);
	return ackdata;
error:
	mutex_unlock(&qos_ipi_mutex);
#endif
	return -1;
}

void qos_ipi_init(void)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	unsigned int ret;
	/* for AP to SSPM */
	ret = mtk_ipi_register(&sspm_ipidev, IPIS_C_QOS, NULL, NULL,
				(void *) &qos_ipi_ackdata);
	if (ret) {
		pr_info("qos IPIS_C_QOS ipi_register fail, ret %d\n", ret);
		qos_sspm_ready = -1;
		return;
	}

	/* for SSPM to AP */
	ret = mtk_ipi_register(&sspm_ipidev, IPIR_I_QOS, NULL, NULL,
				(void *) &qos_recv_ackdata);
	if (ret) {
		pr_info("qos IPIR_I_QOS ipi_register fail, ret %d\n", ret);
		qos_sspm_ready = -2;
		return;
	}
	qos_sspm_ready = 1;
	qos_sspm_enable();
	pr_info("qos ipi is ready!\n");
#endif
}

void qos_ipi_recv_init(void)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	if (qos_sspm_ready != 1) {
		pr_info("QOS SSPM not ready, recv thread not start!\n");
		return;
	}
	kthread_run(qos_ipi_recv_thread, NULL, "qos_ipi_recv");
#endif
}
