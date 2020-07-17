// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kthread.h>
#include <linux/mutex.h>

#if defined(CONFIG_MTK_TINYSYS_SSPM_V2)
#include <sspm_ipi_id.h>
#include <sspm_define.h>
#endif

#include "mtk_qos_common.h"
#include "mtk_qos_bound.h"
#include "mtk_qos_ipi.h"

#if defined(CONFIG_MTK_TINYSYS_SSPM_V2)
static DEFINE_MUTEX(qos_ipi_mutex);
static int qos_sspm_ready;
int qos_ipi_ackdata;
struct qos_ipi_data qos_recv_ackdata;

static int qos_ipi_recv_thread(void *arg)
{
	struct qos_ipi_data *qos_ipi_d;
	int bound_cmd_id = qos_get_ipi_cmd(QOS_IPI_QOS_BOUND);

	pr_info("%s start!\n", __func__);
	do {
		mtk_ipi_recv(&sspm_ipidev, IPIR_I_QOS);

		qos_ipi_d = &qos_recv_ackdata;

		if (qos_ipi_d->cmd == bound_cmd_id)
			qos_notifier_call_chain(
					qos_ipi_d->u.qos_bound.state,
					get_qos_bound());
		else
			pr_info("wrong QoS IPI command: %d\n", qos_ipi_d->cmd);

	} while (!kthread_should_stop());

	return 0;
}
#endif

int qos_ipi_to_sspm_command(void *buffer, int slot)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_V2)
	int ret, ackdata;
	struct qos_ipi_data *qos_ipi_d = buffer;
	int slot_num = sizeof(struct qos_ipi_data)/SSPM_MBOX_SLOT_SIZE;

	mutex_lock(&qos_ipi_mutex);

	ret = qos_get_ipi_cmd(qos_ipi_d->cmd);

	if (qos_ipi_d->cmd < 0) {
		pr_info("qos ipi cmd get error (in= %d, out = %d)\n",
			qos_ipi_d->cmd, ret);
		goto error;
	}

	qos_ipi_d->cmd = ret;

	if (qos_sspm_ready != 1) {
		pr_info("qos ipi not ready, skip cmd=%d\n", qos_ipi_d->cmd);
		goto error;
	}

	pr_info("qos ipi cmd(%d) send\n", qos_ipi_d->cmd);

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
EXPORT_SYMBOL_GPL(qos_ipi_to_sspm_command);

static void qos_sspm_enable(void)
{
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_ENABLE;
	qos_ipi_to_sspm_command(&qos_ipi_d, 1);
}

void qos_ipi_init(struct mtk_qos *qos)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_V2)
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
	pr_info("%s ready!\n", __func__);
#endif
}
EXPORT_SYMBOL_GPL(qos_ipi_init);

void qos_ipi_recv_init(struct mtk_qos *qos)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_V2)
	if (qos_sspm_ready != 1) {
		pr_info("QOS SSPM not ready, recv thread not start!\n");
		return;
	}
	kthread_run(qos_ipi_recv_thread, NULL, "qos_ipi_recv");
#endif
}
EXPORT_SYMBOL_GPL(qos_ipi_recv_init);
