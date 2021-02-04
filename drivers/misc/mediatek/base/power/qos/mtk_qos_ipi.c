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

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_ipi.h>
#include <sspm_ipi_pin.h>
#endif

#include "mtk_qos_bound.h"
#include "mtk_qos_ipi.h"

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
static void qos_sspm_enable(void)
{
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_ENABLE;
	qos_ipi_to_sspm_command(&qos_ipi_d, 1);
}

static int qos_ipi_recv_thread(void *arg)
{
	struct ipi_action qos_isr;
	struct qos_ipi_data qos_ipi_d;
	unsigned int rdata, ret;

	qos_isr.data = &qos_ipi_d;

	ret = sspm_ipi_recv_registration(IPI_ID_QOS, &qos_isr);

	if (ret) {
		pr_err("failed to register sspm recv ipi: %u\n", ret);
		return 0;
	}

	qos_sspm_enable();

	do {
		rdata = 0;
		sspm_ipi_recv_wait(IPI_ID_QOS);

		switch (qos_ipi_d.cmd) {
		case QOS_IPI_QOS_BOUND:
			qos_notifier_call_chain(
					qos_ipi_d.u.qos_bound.state,
					get_qos_bound());
			break;
		default:
			pr_err("wrong QoS IPI command: %d\n", qos_ipi_d.cmd);
		}
	} while (!kthread_should_stop());

	return 0;
}
#endif

int qos_ipi_to_sspm_command(void *buffer, int slot)
{
	int ack_data = 0;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	int ret = sspm_ipi_send_sync(IPI_ID_QOS, IPI_OPT_POLLING,
			buffer, slot, &ack_data, 1);
	if (ret != 0)
		pr_err("qos_ipi_to_sspm error(%d)\n", ret);
#endif
	return ack_data;
}

void qos_ipi_init(void)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	kthread_run(qos_ipi_recv_thread, NULL, "qos_ipi_recv");
#endif
}

