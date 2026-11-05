// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/coresight.h>
#include "coresight-qmi.h"
#include "coresight-priv.h"

DEFINE_CORESIGHT_DEVLIST(qmi_devs, "qmi");

static int service_coresight_qmi_new_server(struct qmi_handle *qmi,
		struct qmi_service *svc)
{
	struct qmi_drvdata *drvdata = container_of(qmi,
					struct qmi_drvdata, handle);

	drvdata->s_addr.sq_family = AF_QIPCRTR;
	drvdata->s_addr.sq_node = svc->node;
	drvdata->s_addr.sq_port = svc->port;
	drvdata->service_connected = true;
	dev_info(drvdata->dev,
		"Connection established between QMI handle and %d service\n",
		drvdata->inst_id);

	return 0;
}

static void service_coresight_qmi_del_server(struct qmi_handle *qmi,
		struct qmi_service *svc)
{
	struct qmi_drvdata *drvdata = container_of(qmi,
					struct qmi_drvdata, handle);
	drvdata->service_connected = false;
	dev_info(drvdata->dev,
		"Connection disconnected between QMI handle and %d service\n",
		drvdata->inst_id);
}

static struct qmi_ops server_ops = {
	.new_server = service_coresight_qmi_new_server,
	.del_server = service_coresight_qmi_del_server,
};

int coresight_qmi_remote_etm_enable(struct coresight_device *csdev)
{
	struct qmi_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);
	struct coresight_set_etm_req_msg_v01 req;
	struct coresight_set_etm_resp_msg_v01 resp = { { 0, 0 } };
	struct qmi_txn txn;
	int ret;

	mutex_lock(&drvdata->mutex);

	if (!drvdata->service_connected) {
		dev_err(drvdata->dev, "QMI service not connected!\n");
		ret = -EINVAL;
		goto err;
	}
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
	req.state = CORESIGHT_ETM_STATE_ENABLED_V01;

	ret = qmi_txn_init(&drvdata->handle, &txn,
			coresight_set_etm_resp_msg_v01_ei,
			&resp);

	if (ret < 0) {
		dev_err(drvdata->dev, "QMI tx init failed , ret:%d\n",
				ret);
		goto err;
	}

	ret = qmi_send_request(&drvdata->handle, &drvdata->s_addr,
			&txn, CORESIGHT_QMI_SET_ETM_REQ_V01,
			CORESIGHT_QMI_SET_ETM_REQ_MAX_LEN,
			coresight_set_etm_req_msg_v01_ei,
			&req);
	if (ret < 0) {
		dev_err(drvdata->dev, "QMI send ACK failed, ret:%d\n",
				ret);
		qmi_txn_cancel(&txn);
		goto err;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(TIMEOUT_MS));
	if (ret < 0) {
		dev_err(drvdata->dev, "QMI qmi txn wait failed, ret:%d\n",
				ret);
		goto err;
	}

	/* Check the response */
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01)
		dev_err(drvdata->dev, "QMI request failed 0x%x\n",
				resp.resp.error);

	mutex_unlock(&drvdata->mutex);

	dev_info(drvdata->dev, "Remote ETM tracing enabled for instance %d\n",
				drvdata->inst_id);
	return 0;
err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(coresight_qmi_remote_etm_enable);

void coresight_qmi_remote_etm_disable(struct coresight_device *csdev)
{
	struct qmi_drvdata *drvdata =
		 dev_get_drvdata(csdev->dev.parent);
	struct coresight_set_etm_req_msg_v01 req;
	struct coresight_set_etm_resp_msg_v01 resp = { { 0, 0 } };
	struct qmi_txn txn;
	int ret;

	mutex_lock(&drvdata->mutex);
	if (!drvdata->service_connected) {
		dev_err(drvdata->dev, "QMI service not connected!\n");
		goto err;
	}

	req.state = CORESIGHT_ETM_STATE_DISABLED_V01;

	ret = qmi_txn_init(&drvdata->handle, &txn,
			coresight_set_etm_resp_msg_v01_ei,
			&resp);

	if (ret < 0) {
		dev_err(drvdata->dev, "QMI tx init failed , ret:%d\n",
				ret);
		goto err;
	}

	ret = qmi_send_request(&drvdata->handle, &drvdata->s_addr,
			&txn, CORESIGHT_QMI_SET_ETM_REQ_V01,
			CORESIGHT_QMI_SET_ETM_REQ_MAX_LEN,
			coresight_set_etm_req_msg_v01_ei,
			&req);
	if (ret < 0) {
		dev_err(drvdata->dev, "QMI send req failed, ret:%d\n",
				 ret);
		qmi_txn_cancel(&txn);
		goto err;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(TIMEOUT_MS));
	if (ret < 0) {
		dev_err(drvdata->dev, "QMI qmi txn wait failed, ret:%d\n",
				ret);
		goto err;
	}

	/* Check the response */
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		dev_err(drvdata->dev, "QMI request failed 0x%x\n",
				resp.resp.error);
		goto err;
	}

	dev_info(drvdata->dev, "Remote ETM tracing disabled for instance %d\n",
				drvdata->inst_id);
err:
	mutex_unlock(&drvdata->mutex);
}
EXPORT_SYMBOL_GPL(coresight_qmi_remote_etm_disable);

/*
 * remote_etm_etr_assign - reassign the ownership of an ETR instance to specified
 * subsystem.
 */
int coresight_qmi_etr_assign(struct coresight_device *csdev,
		struct coresight_etr_assign_req_msg_v01 *req)
{
	struct qmi_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct coresight_etr_assign_resp_msg_v01 resp = { { 0, 0 } };
	struct qmi_txn txn;
	int ret = 0;

	if (!drvdata)
		return -EINVAL;

	mutex_lock(&drvdata->mutex);
	if (!drvdata->service_connected) {
		dev_err(drvdata->dev, "QMI service not connected!\n");
		ret = -EINVAL;
		goto err;
	}
	/*
	 * @subsys_id: id of the subsystem which ownership of etr be assigned.
	 * @etr_id: ETR instance ID.
	 * @buffer_base: Base address of the DDR buffer to be used by this ETR.
	 * @buffer_size: Size in bytes of the DDR buffer to be used by this ETR.
	 */

	ret = qmi_txn_init(&drvdata->handle, &txn,
			coresight_etr_assign_resp_msg_v01_ei,
			&resp);

	if (ret < 0) {
		dev_err(drvdata->dev, "QMI tx init failed , ret:%d\n",
				ret);
		goto err;
	}

	ret = qmi_send_request(&drvdata->handle, &drvdata->s_addr,
			&txn, CORESIGHT_QMI_ETR_ASSIGN_REQ_V01,
			CORESIGHT_QMI_ETR_ASSIGN_REQ_MAX_LEN,
			coresight_etr_assign_req_msg_v01_ei,
			req);
	if (ret < 0) {
		dev_err(drvdata->dev, "QMI send req failed, ret:%d\n",
				 ret);
		qmi_txn_cancel(&txn);
		goto err;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(TIMEOUT_MS));
	if (ret < 0) {
		dev_err(drvdata->dev, "QMI qmi txn wait failed, ret:%d\n",
				ret);
		goto err;
	}

	/* Check the response */
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		dev_err(drvdata->dev, "QMI request failed 0x%x\n",
				resp.resp.error);
		goto err;
	}

	dev_info(drvdata->dev, "Assign etr success\n");
	ret = 0;
err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(coresight_qmi_etr_assign);

int coresight_qmi_assign_atid(struct coresight_device *csdev,
		struct coresight_atid_assign_req_msg_v01 *req)
{
	struct qmi_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);
	struct coresight_atid_assign_resp_msg_v01 resp = { { 0, 0 } };
	struct qmi_txn txn;
	int ret;

	mutex_lock(&drvdata->mutex);

	if (!drvdata->service_connected) {
		dev_err(drvdata->dev, "QMI service not connected!\n");
		ret = -EINVAL;
		goto err;
	}

	/*
	 * The QMI handle may be NULL in the following scenarios:
	 * 1. QMI service is not present
	 * 2. QMI service is present but attempt to enable remote ETM is earlier
	 *    than service is ready to handle request
	 * 3. Connection between QMI client and QMI service failed
	 */

	ret = qmi_txn_init(&drvdata->handle, &txn,
			coresight_atid_assign_resp_msg_v01_ei,
			&resp);

	if (ret < 0) {
		dev_err(drvdata->dev, "QMI tx init failed , ret:%d\n",
				ret);
		goto err;
	}

	ret = qmi_send_request(&drvdata->handle, &drvdata->s_addr,
			&txn, CORESIGHT_QMI_ATID_ASSIGN_V01,
			CORESIGHT_QMI_ATID_ASSIGN_REQ_MAX_LEN,
			coresight_atid_assign_req_msg_v01_ei,
			req);
	if (ret < 0) {
		dev_err(drvdata->dev, "QMI send ACK failed, ret:%d\n",
				ret);
		qmi_txn_cancel(&txn);
		goto err;
	}

	ret = qmi_txn_wait(&txn, msecs_to_jiffies(TIMEOUT_MS));
	if (ret < 0) {
		dev_err(drvdata->dev, "QMI qmi txn wait failed, ret:%d\n",
				ret);
		goto err;
	}

	/* Check the response */
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01)
		dev_err(drvdata->dev, "QMI request failed 0x%x\n",
				resp.resp.error);

	mutex_unlock(&drvdata->mutex);

	dev_info(drvdata->dev, "ATID assign for instance %d\n",
				drvdata->inst_id);
	return 0;
err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(coresight_qmi_assign_atid);

static const struct coresight_ops_helper qmi_helper_ops = {
	.enable = NULL,
	.disable = NULL,
};

static const struct coresight_ops qmi_ops = {
	.helper_ops = &qmi_helper_ops,
};


static int coresight_qmi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct qmi_drvdata *drvdata;
	struct coresight_desc desc = {0 };
	int ret;

	desc.name = coresight_alloc_device_name(&qmi_devs, dev);
	if (!desc.name)
		return -ENOMEM;
	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	pdev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,inst-id",
			&drvdata->inst_id);
	if (ret)
		return ret;

	mutex_init(&drvdata->mutex);

	ret = qmi_handle_init(&drvdata->handle,
			CORESIGHT_QMI_SET_ETM_REQ_MAX_LEN,
			&server_ops, NULL);
	if (ret < 0) {
		dev_err(dev, "qmi client init failed ret:%d\n", ret);
		return ret;
	}

	qmi_add_lookup(&drvdata->handle,
			CORESIGHT_QMI_SVC_ID,
			CORESIGHT_QMI_VERSION,
			drvdata->inst_id);

	desc.type = CORESIGHT_DEV_TYPE_HELPER;
	desc.pdata = pdev->dev.platform_data;
	desc.dev = &pdev->dev;
	desc.ops = &qmi_ops;
	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto err;
	}
	dev_info(dev, "qmi initialized\n");

	return 0;
err:
	qmi_handle_release(&drvdata->handle);
	return ret;
}

static int coresight_qmi_remove(struct platform_device *pdev)
{
	struct qmi_drvdata *drvdata = platform_get_drvdata(pdev);

	qmi_handle_release(&drvdata->handle);
	coresight_unregister(drvdata->csdev);
	return 0;
}

static const struct of_device_id coresight_qmi_match[] = {
	{.compatible = "qcom,coresight-qmi"},
	{}
};

static struct platform_driver coresight_qmi_driver = {
	.probe          = coresight_qmi_probe,
	.remove         = coresight_qmi_remove,
	.driver         = {
		.name   = "coresight-qmi",
		.of_match_table = coresight_qmi_match,
	},
};

int __init coresight_qmi_init(void)
{
	return platform_driver_register(&coresight_qmi_driver);
}
module_init(coresight_qmi_init);

void __exit coresight_qmi_exit(void)
{
	platform_driver_unregister(&coresight_qmi_driver);
}
module_exit(coresight_qmi_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CoreSight QMI driver");

