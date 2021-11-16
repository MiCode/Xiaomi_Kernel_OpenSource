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

#include <helio-dvfsrc-ipi.h>

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
static int qos_recv_thread(void *arg)
{
	struct ipi_action qos_isr;
	struct qos_ipi_data qos_d;
	unsigned int rdata, ret;

	qos_isr.data = &qos_d;

	ret = sspm_ipi_recv_registration(IPI_ID_QOS, &qos_isr);

	if (ret) {
		pr_info("@%s: sspm_ipi_recv_registration failed.\n", __func__);
		return 0;
	}

	while (1) {
		rdata = 0;
		sspm_ipi_recv_wait(IPI_ID_QOS);

		switch (qos_d.cmd) {
		case QOS_IPI_ERROR_HANDLER:
			sspm_ipi_send_ack(IPI_ID_QOS, &rdata);
			break;
		}
	}
	return 0;
}

void helio_dvfsrc_sspm_ipi_init(int dvfsrc_en)
{
	struct qos_ipi_data qos_d;
	struct task_struct *qos_task;

	qos_task = kthread_run(qos_recv_thread, NULL, "qos_recv");

	qos_d.cmd = QOS_IPI_QOS_ENABLE;
	qos_d.u.qos_init.dvfsrc_en = dvfsrc_en;
	qos_ipi_to_sspm_command(&qos_d, 2);
}
#endif

