/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/cpu_pm.h>
#include <linux/pm.h>
#include "adsp_lpm_voting_v01.h"

#define PGS_TIMEOUT			msecs_to_jiffies(3000)

static struct sockaddr_qrtr sq;

static int pgs_vote_send_sync_msg(struct qmi_handle *dev, int vote)
{
	int ret;
	struct prod_set_lpm_vote_req_msg_v01 *req;
	struct prod_set_lpm_vote_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!dev)
		return -ENODEV;

	req = kzalloc(sizeof(struct prod_set_lpm_vote_req_msg_v01), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(struct prod_set_lpm_vote_resp_msg_v01),
			GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->keep_adsp_out_of_lpm = (u8) vote;

	ret = qmi_txn_init(dev, &txn, prod_set_lpm_vote_resp_msg_v01_ei, resp);
	if (ret < 0) {
		pr_err("Failed Init txn for Mode resp %d\n", ret);
		goto out;
	}

	ret = qmi_send_request(dev, &sq, &txn,
			PROD_SET_LPM_VOTE_REQ_V01,
			PROD_SET_LPM_VOTE_REQ_MSG_V01_MAX_MSG_LEN,
			prod_set_lpm_vote_req_msg_v01_ei, req);

	if (ret < 0) {
		qmi_txn_cancel(&txn);
		pr_err("Fail to send Mode req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, PGS_TIMEOUT);

	if (ret < 0) {
		pr_err("Mode resp wait failed with ret %d\n", ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("QMI Mode request rejected, result:%d error:%d\n",
				resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	if (!vote)
		pr_info("ADSP manual unvoting is successful\n");
	else
		pr_info("ADSP manual voting is successful\n");

out:
	kfree(req);
	kfree(resp);
	return ret;
}

static struct qmi_handle adsp_qmi_client;

static int send_adsp_manual_unvote(struct device *dev)
{
	int ret = 0;
	struct qmi_handle *qmi_dev = (struct qmi_handle *) dev_get_drvdata(dev);

	pr_info("ADSP unvoting initiated\n");
	ret = pgs_vote_send_sync_msg(qmi_dev, 0);

	if (ret < 0)
		pr_err("ADSP Unvoting is failed\n");
	return 0;
}

static int send_adsp_manual_vote(struct device *dev)
{
	int ret = 0;
	struct qmi_handle *qmi_dev = (struct qmi_handle *) dev_get_drvdata(dev);

	pr_info("ADSP voting initiated\n");
	ret = pgs_vote_send_sync_msg(qmi_dev, 1);

	if (ret < 0)
		pr_err("ADSP voting is failed\n");
	return 0;
}


static int qmi_adsp_manual_vote_new_server(struct qmi_handle *qmi,
		struct qmi_service *service)
{
	struct platform_device *pdev;
	int ret;

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = service->node;
	sq.sq_port = service->port;

	pdev = platform_device_alloc("qmi_adsp_vote_client",
			PLATFORM_DEVID_AUTO);
	if (!pdev)
		return -ENOMEM;

	ret = platform_device_add_data(pdev, &sq, sizeof(sq));
	if (ret)
		goto err_put_device;
	ret = platform_device_add(pdev);
	if (ret)
		goto err_put_device;

	service->priv = pdev;
	return 0;

err_put_device:
	platform_device_put(pdev);
	return ret;
}

static void qmi_adsp_manual_vote_del_server(struct qmi_handle *qmi,
		struct qmi_service *service)
{
	struct platform_device *pdev = service->priv;

	platform_device_unregister(pdev);
}

static struct qmi_ops adsp_qmi_ops = {
	.new_server = qmi_adsp_manual_vote_new_server,
	.del_server = qmi_adsp_manual_vote_del_server,
};

static const struct dev_pm_ops adsp_manual_vote_dev_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(send_adsp_manual_unvote,
			send_adsp_manual_vote)
};

static int adsp_manual_vote_driver_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct qmi_handle *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	ret = qmi_handle_init(dev,
			PROD_SET_LPM_VOTE_REQ_MSG_V01_MAX_MSG_LEN,
			&adsp_qmi_ops, NULL);
	if (ret < 0) {
		pr_err("Successfully got the qmi_handle for the client\n");
		goto err_handle;
	}

	//Register a new lookup with the service PGS_SERVICE_ID_V01
	ret = qmi_add_lookup(dev, PGS_SERVICE_ID_V01,
			PGS_SERVICE_VERS_V01, 0);

	if (ret < 0)
		goto err_handle;

	platform_set_drvdata(pdev, (void *) dev);
	pr_info("ADSP vote device is registered successfully\n");
	return ret;
err_handle:
	kfree(dev);
	return ret;
}

static int adsp_manual_vote_driver_remove(struct platform_device *pdev)
{
	struct qmi_handle *dev = (struct qmi_handle *)
					platform_get_drvdata(pdev);

	qmi_handle_release(dev);
	return 0;
}

static struct platform_device adsp_manual_vote_device = {
	.name = "adsp_manual_vote",
	.id = -1,
};

static struct platform_driver adsp_manual_vote_driver = {
	.probe = adsp_manual_vote_driver_probe,
	.remove = adsp_manual_vote_driver_remove,
	.driver = {
		.name = "adsp_manual_vote",
		.owner = THIS_MODULE,
		.pm = &adsp_manual_vote_dev_pm_ops,
	},
};

static int __init adsp_manual_vote_qmi_init(void)
{
	int ret = 0;

	ret = platform_device_register(&adsp_manual_vote_device);
	if (ret)
		return -ENODEV;

	pr_info("ADSP vote device is registered successfully\n");

	ret = platform_driver_register(&adsp_manual_vote_driver);
	if (ret) {
		pr_err("%s: fail to register ADSP manual vote device driver\n",
				__func__);
		goto out;
	}
out:
	return ret;
}

static void __exit adsp_manual_vote_qmi_deinit(void)
{
	qmi_handle_release(&adsp_qmi_client);
	platform_driver_unregister(&adsp_manual_vote_driver);
	platform_device_unregister(&adsp_manual_vote_device);
}

module_init(adsp_manual_vote_qmi_init);
module_exit(adsp_manual_vote_qmi_deinit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ADSP Manual vote QMI client driver");
