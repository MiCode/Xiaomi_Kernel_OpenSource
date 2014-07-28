/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>
#include "coresight-qmi.h"

#ifdef CONFIG_CORESIGHT_MODEM_ETM_DEFAULT_ENABLE
static int boot_enable = 1;
#else
static int boot_enable;
#endif
module_param_named(
	boot_enable, boot_enable, int, S_IRUGO
);

struct modem_etm_drvdata {
	struct device			*dev;
	struct coresight_device		*csdev;
	struct mutex			mutex;
	struct workqueue_struct		*wq;
	struct qmi_handle		*handle;
	struct work_struct		work_svc_arrive;
	struct work_struct		work_svc_exit;
	struct work_struct		work_rcv_msg;
	struct notifier_block		nb;
};

static int modem_etm_enable(struct coresight_device *csdev)
{
	struct modem_etm_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);
	struct coresight_set_etm_req_msg_v01 req;
	struct coresight_set_etm_resp_msg_v01 resp = { { 0, 0 } };
	struct msg_desc req_desc, resp_desc;
	int ret;

	mutex_lock(&drvdata->mutex);

	/*
	 * The QMI handle may be NULL in the following scenarios:
	 * 1. QMI service is not present
	 * 2. QMI service is present but attempt to enable remote ETM is earlier
	 *    than service is ready to handle request
	 * 3. Connection between QMI client and QMI service failed
	 *
	 * Enable CoreSight without processing further QMI commands which
	 * provides the option to enable remote ETM by other means.
	 */
	if (!drvdata->handle) {
		dev_info(drvdata->dev,
			 "%s: QMI service unavailable. Skipping QMI requests\n",
			 __func__);
		goto out;
	}

	req.state = CORESIGHT_ETM_STATE_ENABLED_V01;

	req_desc.msg_id = CORESIGHT_QMI_SET_ETM_REQ_V01;
	req_desc.max_msg_len = CORESIGHT_QMI_SET_ETM_REQ_MAX_LEN;
	req_desc.ei_array = coresight_set_etm_req_msg_v01_ei;

	resp_desc.msg_id = CORESIGHT_QMI_SET_ETM_RESP_V01;
	resp_desc.max_msg_len = CORESIGHT_QMI_SET_ETM_RESP_MAX_LEN;
	resp_desc.ei_array = coresight_set_etm_resp_msg_v01_ei;

	ret = qmi_send_req_wait(drvdata->handle, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp), TIMEOUT_MS);

	if (ret < 0) {
		dev_err(drvdata->dev, "%s: QMI send req failed %d\n", __func__,
			ret);
		goto err;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		dev_err(drvdata->dev, "%s: QMI request failed %d %d\n",
			__func__, resp.resp.result, resp.resp.error);
		ret = -EREMOTEIO;
		goto err;
	}
out:
	mutex_unlock(&drvdata->mutex);

	dev_info(drvdata->dev, "Modem ETM tracing enabled\n");
	return 0;
err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}

static void modem_etm_disable(struct coresight_device *csdev)
{
	struct modem_etm_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);
	struct coresight_set_etm_req_msg_v01 req;
	struct coresight_set_etm_resp_msg_v01 resp = { { 0, 0 } };
	struct msg_desc req_desc, resp_desc;
	int ret;

	mutex_lock(&drvdata->mutex);

	if (!drvdata->handle) {
		dev_info(drvdata->dev,
			 "%s: QMI service unavailable. Skipping QMI requests\n",
			 __func__);
		goto out;
	}

	req.state = CORESIGHT_ETM_STATE_DISABLED_V01;

	req_desc.msg_id = CORESIGHT_QMI_SET_ETM_REQ_V01;
	req_desc.max_msg_len = CORESIGHT_QMI_SET_ETM_REQ_MAX_LEN;
	req_desc.ei_array = coresight_set_etm_req_msg_v01_ei;

	resp_desc.msg_id = CORESIGHT_QMI_SET_ETM_RESP_V01;
	resp_desc.max_msg_len = CORESIGHT_QMI_SET_ETM_RESP_MAX_LEN;
	resp_desc.ei_array = coresight_set_etm_resp_msg_v01_ei;

	ret = qmi_send_req_wait(drvdata->handle, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp), TIMEOUT_MS);
	if (ret < 0) {
		dev_err(drvdata->dev, "%s: QMI send req failed %d\n", __func__,
			ret);
		goto err;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		dev_err(drvdata->dev, "%s: QMI request failed %d %d\n",
			__func__, resp.resp.result, resp.resp.error);
		goto err;
	}
out:
	mutex_unlock(&drvdata->mutex);

	dev_info(drvdata->dev, "Modem ETM tracing disabled\n");
	return;
err:
	mutex_unlock(&drvdata->mutex);
}

static const struct coresight_ops_source modem_etm_source_ops = {
	.enable		= modem_etm_enable,
	.disable	= modem_etm_disable,
};

static const struct coresight_ops modem_cs_ops = {
	.source_ops	= &modem_etm_source_ops,
};

static void modem_etm_rcv_msg(struct work_struct *work)
{
	struct modem_etm_drvdata *drvdata = container_of(work,
					    struct modem_etm_drvdata,
					    work_rcv_msg);

	if (qmi_recv_msg(drvdata->handle) < 0)
		dev_err(drvdata->dev, "%s: Error receiving QMI message\n",
			__func__);
}

static void modem_etm_notify(struct qmi_handle *handle,
			enum qmi_event_type event, void *notify_priv)
{
	struct modem_etm_drvdata *drvdata =
			(struct modem_etm_drvdata *)notify_priv;
	switch (event) {
	case QMI_RECV_MSG:
		queue_work(drvdata->wq, &drvdata->work_rcv_msg);
		break;
	default:
		break;
	}
}

static void modem_etm_svc_arrive(struct work_struct *work)
{
	struct modem_etm_drvdata *drvdata = container_of(work,
					    struct modem_etm_drvdata,
					    work_svc_arrive);

	drvdata->handle = qmi_handle_create(modem_etm_notify, drvdata);
	if (!drvdata->handle) {
		dev_err(drvdata->dev, "%s: QMI client handle alloc failed\n",
			__func__);
		return;
	}

	if (qmi_connect_to_service(drvdata->handle, CORESIGHT_QMI_SVC_ID,
				   CORESIGHT_QMI_VERSION,
				   CORESIGHT_SVC_INST_ID_MODEM_V01) < 0) {
		dev_err(drvdata->dev,
			"%s: Could not connect handle to service\n", __func__);
		qmi_handle_destroy(drvdata->handle);
		drvdata->handle = NULL;
	}
}

static void modem_etm_svc_exit(struct work_struct *work)
{
	struct modem_etm_drvdata *drvdata = container_of(work,
					    struct modem_etm_drvdata,
					    work_svc_exit);

	qmi_handle_destroy(drvdata->handle);
	drvdata->handle = NULL;
}

static int modem_etm_svc_event_notify(struct notifier_block *this,
				      unsigned long event,
				      void *data)
{
	struct modem_etm_drvdata *drvdata = container_of(this,
					    struct modem_etm_drvdata,
					    nb);

	switch (event) {
	case QMI_SERVER_ARRIVE:
		queue_work(drvdata->wq, &drvdata->work_svc_arrive);
		break;
	case QMI_SERVER_EXIT:
		queue_work(drvdata->wq, &drvdata->work_svc_exit);
		break;
	default:
		break;
	}
	return 0;
}

static int modem_etm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct modem_etm_drvdata *drvdata;
	struct coresight_desc *desc;
	int ret;

	if (pdev->dev.of_node) {
		pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
		pdev->dev.platform_data = pdata;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	mutex_init(&drvdata->mutex);

	drvdata->nb.notifier_call = modem_etm_svc_event_notify;

	drvdata->wq = create_singlethread_workqueue("modem-etm");
	if (!drvdata->wq)
		return -EFAULT;
	INIT_WORK(&drvdata->work_svc_arrive, modem_etm_svc_arrive);
	INIT_WORK(&drvdata->work_svc_exit, modem_etm_svc_exit);
	INIT_WORK(&drvdata->work_rcv_msg, modem_etm_rcv_msg);
	ret = qmi_svc_event_notifier_register(CORESIGHT_QMI_SVC_ID,
					      CORESIGHT_QMI_VERSION,
					      CORESIGHT_SVC_INST_ID_MODEM_V01,
					      &drvdata->nb);
	if (ret < 0)
		goto err0;

	desc->type = CORESIGHT_DEV_TYPE_SOURCE;
	desc->subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_PROC;
	desc->ops = &modem_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto err1;
	}
	dev_info(dev, "Modem ETM initialized\n");

	if (boot_enable)
		coresight_enable(drvdata->csdev);
	return 0;
err1:
	qmi_svc_event_notifier_unregister(CORESIGHT_QMI_SVC_ID,
					  CORESIGHT_QMI_VERSION,
					  CORESIGHT_SVC_INST_ID_MODEM_V01,
					  &drvdata->nb);
err0:
	destroy_workqueue(drvdata->wq);
	return ret;
}

static int modem_etm_remove(struct platform_device *pdev)
{
	struct modem_etm_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id modem_etm_match[] = {
	{.compatible = "qcom,coresight-modem-etm"},
	{}
};

static struct platform_driver modem_etm_driver = {
	.probe          = modem_etm_probe,
	.remove         = modem_etm_remove,
	.driver         = {
		.name   = "coresight-modem-etm",
		.owner	= THIS_MODULE,
		.of_match_table = modem_etm_match,
	},
};

int __init modem_etm_init(void)
{
	return platform_driver_register(&modem_etm_driver);
}
module_init(modem_etm_init);

void __exit modem_etm_exit(void)
{
	platform_driver_unregister(&modem_etm_driver);
}
module_exit(modem_etm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Modem ETM driver");
