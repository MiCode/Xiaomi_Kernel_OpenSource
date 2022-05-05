// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/of.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2)
#include <sspm_ipi_id.h>
#include <sspm_define.h>
#endif

#include "mtk_qos_common.h"
#include "mtk_qos_bound.h"
#include "mtk_qos_ipi.h"
#include <linux/scmi_protocol.h>
#include <linux/module.h>
#include <tinysys-scmi.h>


static int scmi_qos_id;
static struct scmi_tinysys_info_st *_tinfo;
static DEFINE_MUTEX(qos_ipi_mutex);
static int qos_sspm_ready;
int qos_ipi_ackdata;
struct qos_ipi_data qos_recv_ackdata;

static void qos_scmi_handler(u32 r_feature_id, scmi_tinysys_report *report)
{
	unsigned int cmd, arg;


	/* need fix */
	//mtk_ipi_recv(&sspm_ipidev, IPIR_I_QOS);

		cmd = report->p1;
		arg = report->p2;

		if (cmd == QOS_IPI_QOS_BOUND)
			qos_notifier_call_chain(
					arg,
					get_qos_bound());
		else
			pr_info("wrong QoS IPI command: %d\n", cmd);


		return;
}

int qos_ipi_to_sspm_scmi_command(unsigned int cmd, unsigned int p1, unsigned int p2,
			unsigned int p3,unsigned int p4)
{

	int ret = 0, ackdata = 0;
	struct scmi_tinysys_status rvalue = {0};

	mutex_lock(&qos_ipi_mutex);

	if (cmd >= NR_QOS_IPI) {
		pr_info("qos ipi cmd get error %d\n",
			cmd);
		goto error;
	}
	if (qos_sspm_ready != 1) {
		pr_info("qos ipi not ready, skip cmd=%d\n", cmd);
		goto error;
	}
	switch (p4) {
		case QOS_IPI_SCMI_SET:
			ret = scmi_tinysys_common_set(_tinfo->ph, scmi_qos_id,
				cmd, p1, p2, p3, p4);
			if (ret) {
				pr_info("qos ipi cmd %d send fail ret %d\n",
				cmd, ret);
				goto error;
			}
			pr_info("qos send ipi to sspm cmd %d success\n", cmd);
			ackdata = rvalue.r1;
			break;
		case QOS_IPI_SCMI_GET:
			ret = scmi_tinysys_common_get(_tinfo->ph,
					      scmi_qos_id, cmd, &rvalue);
			if (ret) {
				pr_info("qos ipi cmd %d ack fail ret %d return_val %d %d\n",
				cmd, ret, rvalue.r1, rvalue.r2);
				goto error;
			}
			ackdata = rvalue.r1;
			pr_info("QOS_IPI_SCMI_GET, ackdata %d, %d, %d, ret %d\n",
		        	rvalue.r1, rvalue.r2, rvalue.r3, ret);

			if (!ackdata) {
				pr_info("qos ipi cmd %d ack fail, ackdata=%d\n",
				cmd, ackdata);
				goto error;
			}
			break;
	}
	mutex_unlock(&qos_ipi_mutex);
	return ackdata;
error:
	mutex_unlock(&qos_ipi_mutex);
	return -1;

}

EXPORT_SYMBOL_GPL(qos_ipi_to_sspm_scmi_command);

static void qos_sspm_enable(void)
{
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_ENABLE;
	qos_ipi_to_sspm_scmi_command(qos_ipi_d.cmd, 0, 0, 0, 0);
}

void qos_ipi_init(struct mtk_qos *qos)
{
	unsigned int ret;

	_tinfo = get_scmi_tinysys_info();

	ret = of_property_read_u32(_tinfo->sdev->dev.of_node, "scmi_qos",
			&scmi_qos_id);
	if (ret) {
		pr_info("get scmi_qos fail, ret %d\n", ret);
		qos_sspm_ready = -2;
		return;
	}

	scmi_tinysys_register_event_notifier(scmi_qos_id,
		(f_handler_t)qos_scmi_handler);
	ret = scmi_tinysys_event_notify(scmi_qos_id, 1);
	if (ret)
		pr_info("qos event notify fail ...");

	qos_sspm_ready = 1;
	qos_sspm_enable();
	pr_info("%s ready!\n", __func__);
	
}
EXPORT_SYMBOL_GPL(qos_ipi_init);

void qos_ipi_recv_init(struct mtk_qos *qos)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2)
	if (qos_sspm_ready != 1) {
		pr_info("QOS SSPM not ready, recv thread not start!\n");
		return;
	}
	kthread_run(qos_ipi_recv_thread, NULL, "qos_ipi_recv");
#endif
}
EXPORT_SYMBOL_GPL(qos_ipi_recv_init);
