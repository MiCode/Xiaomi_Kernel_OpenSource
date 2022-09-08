// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kthread.h>
#include <linux/mutex.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V1)
#include <sspm_ipi.h>
#include <sspm_ipi_pin.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V0)
#include <sspm_ipi.h>
#include <sspm_ipi_pin.h>
#endif

#include "mtk_qos_common.h"
#include "mtk_qos_bound.h"
#include "mtk_qos_ipi.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V1) || IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V0)
static int qos_sspm_ready;

static int qos_ipi_recv_thread(void *arg)
{
	struct ipi_action qos_isr;
	struct qos_ipi_data qos_ipi_d;
	unsigned int rdata, ret;
	int bound_cmd_id = qos_get_ipi_cmd(QOS_IPI_QOS_BOUND);

	qos_isr.data = &qos_ipi_d;

	ret = sspm_ipi_recv_registration(IPI_ID_QOS, &qos_isr);

	if (ret) {
		pr_info("@%s: sspm_ipi_recv_registration failed.\n", __func__);
		return 0;
	}

	do {
		rdata = 0;
		sspm_ipi_recv_wait(IPI_ID_QOS);

		if (qos_ipi_d.cmd == bound_cmd_id)
			qos_notifier_call_chain(
					qos_ipi_d.u.qos_bound.state,
					get_qos_bound());
		else
			pr_info("mtkqos: %s wrong QoS IPI command: %d\n",
				__func__, qos_ipi_d.cmd);

	} while (!kthread_should_stop());
	return 0;
}
#endif

int qos_ipi_to_sspm_command(void *buffer, int slot)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V1) || IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V0)
	int ack_data = 0;
	struct qos_ipi_data *qos_ipi_d = buffer;

	int ret = qos_get_ipi_cmd(qos_ipi_d->cmd);

	if (qos_ipi_d->cmd < 0) {
		pr_info("qos ipi cmd get error (in= %d, out = %d)\n",
			qos_ipi_d->cmd, ret);
		return -1;
	}

	qos_ipi_d->cmd = ret;

	sspm_ipi_send_sync(IPI_ID_QOS, IPI_OPT_POLLING,
			buffer, slot, &ack_data, 1);

	return ack_data;
#else
	return 0;
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
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V1) || IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V0)
	qos_sspm_ready = 1;
	qos_sspm_enable();
	pr_info("%s ready!\n", __func__);
#endif
}
EXPORT_SYMBOL_GPL(qos_ipi_init);

void qos_ipi_recv_init(struct mtk_qos *qos)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V1) || IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V0)
	if (qos_sspm_ready != 1) {
		pr_info("QOS SSPM not ready, recv thread not start!\n");
		return;
	}
	kthread_run(qos_ipi_recv_thread, NULL, "qos_ipi_recv");
#endif
}
EXPORT_SYMBOL_GPL(qos_ipi_recv_init);
