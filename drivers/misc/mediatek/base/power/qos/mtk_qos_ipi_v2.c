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
#include <sspm_ipi_id.h>
#endif

#include "mtk_qos_bound.h"
#include "mtk_qos_ipi.h"

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
int qos_ipi_ackdata;
int qos_recv_ackdata;

static void qos_sspm_enable(void)
{
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_ENABLE;
	qos_ipi_to_sspm_command(&qos_ipi_d, 1);
}

static int qos_ipi_recv_thread(void *arg)
{
	struct qos_ipi_data qos_ipi_d;
	unsigned int rdata, ret;

	/* for AP to SSPM */
	ret = mtk_ipi_register(&sspm_ipidev, IPIS_C_QOS, NULL, NULL,
				(void *) &qos_ipi_ackdata);
	if (ret) {
		pr_err("[SSPM] IPIS_C_QOS ipi_register fail, ret %d\n", ret);
		return -1;
	}

	/* for SSPM to AP */
	ret = mtk_ipi_register(&sspm_ipidev, IPIR_I_QOS, NULL, NULL,
				(void *) &qos_recv_ackdata);
	if (ret) {
		pr_err("[SSPM] IPIR_I_QOS ipi_register fail, ret %d\n", ret);
		return -1;
	}
	pr_info("SSPM is ready to service IPI\n");

	qos_sspm_enable();

	do {
		rdata = 0;
		mtk_ipi_recv(&sspm_ipidev, IPIR_I_QOS);

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
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	int ret;

	qos_ipi_ackdata = 0;

	ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_QOS,
		IPI_SEND_POLLING, buffer,
		slot, 10);
	if (ret) {
		pr_err("SSPM: plt IPI fail ret=%d\n", ret);
		goto error;
	}

	if (!qos_ipi_ackdata) {
		pr_err("SSPM: plt IPI init fail, ackdata=%d\n",
		qos_ipi_ackdata);
		goto error;
	}


#endif
	return 0;
error:
	return -1;
}

void qos_ipi_init(void)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	kthread_run(qos_ipi_recv_thread, NULL, "qos_ipi_recv");
#endif
}
