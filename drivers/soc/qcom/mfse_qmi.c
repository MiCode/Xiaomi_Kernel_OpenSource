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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <soc/qcom/msm_qmi_interface.h>

#include "mfse_qmi_v01.h"

#define MFSE_SERVICE_INS_ID	0
#define MFSE_TIMEOUT_MS		5000

struct mfse_qmi_dev {
	struct mutex mutex;
	struct qmi_handle *handle;
	struct work_struct svc_arrive_work;
	struct work_struct svc_exit_work;
	struct work_struct qmi_recv_msg_work;
	struct notifier_block qmi_clnt_nb;
};

struct mfse_qmi_dev *qmi_dev;
struct class *mfse_class;
struct device *device;

static ssize_t value_show(struct device *pdev, struct device_attribute *attr,
				char *buf)
{
	struct mfse_qmi_dev *dev = qmi_dev;
	struct mfse_get_efs_sync_timer_req_msg_v01 req;
	struct mfse_get_efs_sync_timer_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int ret = -EINVAL;

	mutex_lock(&dev->mutex);
	if (!dev->handle) {
		pr_err("%s: QMI service unavailable\n", __func__);
		goto err;
	}

	memset(&req, 0, sizeof(struct mfse_get_efs_sync_timer_req_msg_v01));
	memset(&resp, 0, sizeof(struct mfse_get_efs_sync_timer_resp_msg_v01));

	req.fs_id = MFSE_EFS2_V01;

	req_desc.msg_id = QMI_MFSE_GET_EFS_SYNC_TIMER_REQ_V01;
	req_desc.max_msg_len = MFSE_GET_EFS_SYNC_TIMER_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.ei_array = mfse_get_efs_sync_timer_req_msg_v01_ei;

	resp_desc.msg_id = QMI_MFSE_GET_EFS_SYNC_TIMER_RESP_V01;
	resp_desc.max_msg_len =
		MFSE_GET_EFS_SYNC_TIMER_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.ei_array = mfse_get_efs_sync_timer_resp_msg_v01_ei;

	ret = qmi_send_req_wait(dev->handle, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), MFSE_TIMEOUT_MS);
	if (ret < 0) {
		pr_err("%s: QMI send req failed %d\n", __func__, ret);
		goto err;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s: QMI request failed %d %d\n",
				__func__, resp.resp.result, resp.resp.error);
		ret = -EREMOTEIO;
		goto err;
	}

	mutex_unlock(&dev->mutex);

	return snprintf(buf, PAGE_SIZE, "Current efs sync timer value: %d\n",
						resp.efs_timer_value);

err:
	mutex_unlock(&dev->mutex);
	return ret;
}

static ssize_t value_store(struct device *pdev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct mfse_qmi_dev *dev = qmi_dev;
	struct mfse_set_efs_sync_timer_req_msg_v01 req;
	struct mfse_set_efs_sync_timer_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	uint32_t efs_timer_stats;
	int ret = -EINVAL;

	if (buf == NULL) {
		pr_err("%s: Invalid input buffer\n", __func__);
		goto err1;
	}

	if (kstrtouint(buf, 0, &efs_timer_stats)) {
		pr_err("%s: Please enter positive integer\n", __func__);
		goto err1;
	}

	mutex_lock(&dev->mutex);
	if (!dev->handle) {
		pr_err("%s: QMI service unavailable\n", __func__);
		goto err2;
	}

	memset(&req, 0, sizeof(struct mfse_set_efs_sync_timer_req_msg_v01));
	memset(&resp, 0, sizeof(struct mfse_set_efs_sync_timer_resp_msg_v01));

	req.fs_id = MFSE_EFS2_V01;
	req.efs_timer_value = efs_timer_stats;

	req_desc.msg_id = QMI_MFSE_SET_EFS_SYNC_TIMER_REQ_V01;
	req_desc.max_msg_len = MFSE_SET_EFS_SYNC_TIMER_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.ei_array = mfse_set_efs_sync_timer_req_msg_v01_ei;

	resp_desc.msg_id = QMI_MFSE_SET_EFS_SYNC_TIMER_RESP_V01;
	resp_desc.max_msg_len =
		MFSE_SET_EFS_SYNC_TIMER_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.ei_array = mfse_set_efs_sync_timer_resp_msg_v01_ei;

	ret = qmi_send_req_wait(dev->handle, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), MFSE_TIMEOUT_MS);
	if (ret < 0) {
		pr_err("%s: QMI send req failed %d\n", __func__, ret);
		goto err2;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		pr_err("%s: QMI request failed %d %d\n",
			__func__, resp.resp.result, resp.resp.error);
		ret = -EREMOTEIO;
		goto err2;
	}

	mutex_unlock(&dev->mutex);

	return size;

err2:
	mutex_unlock(&dev->mutex);
err1:
	return ret;
}

static DEVICE_ATTR(value, 0644, value_show, value_store);

static struct device_attribute *attr = &dev_attr_value;

static void mfse_qmi_clnt_notifier_work(struct work_struct *work)
{
	struct mfse_qmi_dev *dev = container_of(work, struct mfse_qmi_dev,
					qmi_recv_msg_work);

	if (qmi_recv_msg(dev->handle) < 0)
		pr_err("%s: Error receiving QMI msg\n", __func__);
}

static void mfse_qmi_notify(struct qmi_handle *handle,
			enum qmi_event_type event, void *notify_priv)
{
	struct mfse_qmi_dev *dev = (struct mfse_qmi_dev *)notify_priv;

	switch (event) {
	case QMI_RECV_MSG:
		schedule_work(&dev->qmi_recv_msg_work);
		break;
	default:
		break;
	}
}

static void mfse_qmi_svc_arrive_work(struct work_struct *work)
{
	struct mfse_qmi_dev *dev = container_of(work, struct mfse_qmi_dev,
					svc_arrive_work);

	mutex_lock(&dev->mutex);
	dev->handle = qmi_handle_create(mfse_qmi_notify, dev);
	if (!dev->handle) {
		pr_err("%s: QMI client handle alloc failed\n", __func__);
		mutex_unlock(&dev->mutex);
		return;
	}

	if (qmi_connect_to_service(dev->handle, MFSE_SERVICE_ID_V01,
				MFSE_SERVICE_VERS_V01, MFSE_SERVICE_INS_ID)) {
		pr_err("%s: Could not connect handle to service\n", __func__);
		qmi_handle_destroy(dev->handle);
		dev->handle = NULL;
		mutex_unlock(&dev->mutex);
		return;
	}

	pr_debug("QMI handle connected to MFSE service\n");
	mutex_unlock(&dev->mutex);
}

static void mfse_qmi_svc_exit_work(struct work_struct *work)
{
	struct mfse_qmi_dev *dev = container_of(work, struct mfse_qmi_dev,
					svc_exit_work);

	mutex_lock(&dev->mutex);
	qmi_handle_destroy(dev->handle);
	dev->handle = NULL;
	pr_debug("MFSE QMI handle destroyed\n");
	mutex_unlock(&dev->mutex);
}

static int mfse_qmi_clnt_svc_event_notifier(struct notifier_block *nb,
					unsigned long code, void *_cmd)
{
	struct mfse_qmi_dev *dev = container_of(nb, struct mfse_qmi_dev,
					qmi_clnt_nb);

	switch (code) {
	case QMI_SERVER_ARRIVE:
		schedule_work(&dev->svc_arrive_work);
		break;
	case QMI_SERVER_EXIT:
		schedule_work(&dev->svc_exit_work);
		break;
	}

	return 0;
}

static int __init mfse_qmi_init(void)
{
	int ret = 0;
	struct mfse_qmi_dev *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	mfse_class = class_create(THIS_MODULE, "mfse_qmi_clnt");
	if (IS_ERR(mfse_class)) {
		ret = PTR_ERR(mfse_class);
		pr_err("%s: class_create failed: %d\n", __func__, ret);
		goto err_class;
	}

	device = device_create(mfse_class, NULL,
				MKDEV(0, 0), NULL, "sync_timer");
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("%s: Failed to create device: %d\n", __func__, ret);
		goto err_device;
	}

	ret = device_create_file(device, attr);
	if (ret) {
		pr_err("%s: Failed to create timer file: %d", __func__, ret);
		goto err_attr;
	}

	mutex_init(&dev->mutex);
	INIT_WORK(&dev->svc_arrive_work, mfse_qmi_svc_arrive_work);
	INIT_WORK(&dev->svc_exit_work, mfse_qmi_svc_exit_work);
	INIT_WORK(&dev->qmi_recv_msg_work, mfse_qmi_clnt_notifier_work);
	dev->qmi_clnt_nb.notifier_call = mfse_qmi_clnt_svc_event_notifier;

	ret = qmi_svc_event_notifier_register(MFSE_SERVICE_ID_V01,
					      MFSE_SERVICE_VERS_V01,
					      MFSE_SERVICE_INS_ID,
					      &dev->qmi_clnt_nb);
	if (ret < 0) {
		pr_err("%s: Notifier register failed ret: %d\n", __func__, ret);
		goto err_notifier;
	}

	qmi_dev = dev;
	pr_info("MFSE QMI client module initialized\n");

	return 0;

err_notifier:
	device_remove_file(device, attr);
err_attr:
	device_destroy(device->class, device->devt);
err_device:
	class_destroy(mfse_class);
err_class:
	kfree(dev);
	return ret;
}

static void __exit mfse_qmi_exit(void)
{
	struct mfse_qmi_dev *dev = qmi_dev;

	qmi_svc_event_notifier_unregister(MFSE_SERVICE_ID_V01,
					  MFSE_SERVICE_VERS_V01,
					  MFSE_SERVICE_INS_ID,
					  &dev->qmi_clnt_nb);
	device_remove_file(device, attr);
	device_destroy(device->class, device->devt);
	class_destroy(mfse_class);
	qmi_dev = NULL;
	kfree(dev);
}

module_init(mfse_qmi_init);
module_exit(mfse_qmi_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MFSE QMI client driver");
