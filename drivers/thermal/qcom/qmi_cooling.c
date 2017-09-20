/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <soc/qcom/msm_qmi_interface.h>

#include "thermal_mitigation_device_service_v01.h"

#define QMI_CDEV_DRIVER		"qmi-cooling-device"
#define QMI_TMD_RESP_TOUT_MSEC	50
#define QMI_CLIENT_NAME_LENGTH	40

enum qmi_device_type {
	QMI_CDEV_MAX_LIMIT_TYPE,
	QMI_CDEV_MIN_LIMIT_TYPE,
	QMI_CDEV_TYPE_NR,
};

struct qmi_cooling_device {
	struct device_node		*np;
	char				cdev_name[THERMAL_NAME_LENGTH];
	char				qmi_name[QMI_CLIENT_NAME_LENGTH];
	bool                            connection_active;
	enum qmi_device_type		type;
	struct list_head		qmi_node;
	struct thermal_cooling_device	*cdev;
	unsigned int			mtgn_state;
	unsigned int			max_level;
	struct qmi_tmd_instance		*tmd;
};

struct qmi_tmd_instance {
	struct device			*dev;
	struct qmi_handle		*handle;
	struct mutex			mutex;
	struct work_struct		work_svc_arrive;
	struct work_struct		work_svc_exit;
	struct work_struct		work_rcv_msg;
	struct notifier_block		nb;
	uint32_t			inst_id;
	struct list_head		tmd_cdev_list;
};

struct qmi_dev_info {
	char				*dev_name;
	enum qmi_device_type		type;
};

static struct workqueue_struct *qmi_tmd_wq;
static struct qmi_tmd_instance *tmd_instances;
static int tmd_inst_cnt;

static struct qmi_dev_info device_clients[] = {
	{
		.dev_name = "pa",
		.type = QMI_CDEV_MAX_LIMIT_TYPE,
	},
	{
		.dev_name = "cx_vdd_limit",
		.type = QMI_CDEV_MAX_LIMIT_TYPE,
	},
	{
		.dev_name = "modem",
		.type = QMI_CDEV_MAX_LIMIT_TYPE,
	},
	{
		.dev_name = "modem_current",
		.type = QMI_CDEV_MAX_LIMIT_TYPE,
	},
	{
		.dev_name = "modem_skin",
		.type = QMI_CDEV_MAX_LIMIT_TYPE,
	},
	{
		.dev_name = "modem_bw",
		.type = QMI_CDEV_MAX_LIMIT_TYPE,
	},
	{
		.dev_name = "cpuv_restriction_cold",
		.type = QMI_CDEV_MIN_LIMIT_TYPE,
	},
	{
		.dev_name = "cpr_cold",
		.type = QMI_CDEV_MIN_LIMIT_TYPE,
	}
};

static int qmi_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct qmi_cooling_device *qmi_cdev = cdev->devdata;

	if (!qmi_cdev)
		return -EINVAL;

	*state = qmi_cdev->max_level;

	return 0;
}

static int qmi_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct qmi_cooling_device *qmi_cdev = cdev->devdata;

	if (!qmi_cdev)
		return -EINVAL;

	if (qmi_cdev->type == QMI_CDEV_MIN_LIMIT_TYPE) {
		*state = 0;
		return 0;
	}
	*state = qmi_cdev->mtgn_state;

	return 0;
}

static int qmi_tmd_send_state_request(struct qmi_cooling_device *qmi_cdev,
				uint8_t state)
{
	int ret = 0;
	struct tmd_set_mitigation_level_req_msg_v01 req;
	struct tmd_set_mitigation_level_resp_msg_v01 tmd_resp;
	struct msg_desc req_desc, resp_desc;
	struct qmi_tmd_instance *tmd = qmi_cdev->tmd;

	memset(&req, 0, sizeof(req));
	memset(&tmd_resp, 0, sizeof(tmd_resp));

	strlcpy(req.mitigation_dev_id.mitigation_dev_id, qmi_cdev->qmi_name,
		QMI_TMD_MITIGATION_DEV_ID_LENGTH_MAX_V01);
	req.mitigation_level = state;

	req_desc.max_msg_len = TMD_SET_MITIGATION_LEVEL_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_TMD_SET_MITIGATION_LEVEL_REQ_V01;
	req_desc.ei_array = tmd_set_mitigation_level_req_msg_v01_ei;

	resp_desc.max_msg_len =
		TMD_SET_MITIGATION_LEVEL_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_TMD_SET_MITIGATION_LEVEL_RESP_V01;
	resp_desc.ei_array = tmd_set_mitigation_level_resp_msg_v01_ei;

	mutex_lock(&tmd->mutex);
	ret = qmi_send_req_wait(tmd->handle,
				&req_desc, &req, sizeof(req),
				&resp_desc, &tmd_resp, sizeof(tmd_resp),
				QMI_TMD_RESP_TOUT_MSEC);
	if (ret < 0) {
		pr_err("qmi set state:%d failed for %s ret:%d\n",
			state, qmi_cdev->cdev_name, ret);
		goto qmi_send_exit;
	}

	if (tmd_resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = tmd_resp.resp.result;
		pr_err("qmi set state:%d NOT success for %s ret:%d\n",
			state, qmi_cdev->cdev_name, ret);
		goto qmi_send_exit;
	}
	pr_debug("Requested qmi state:%d for %s\n", state, qmi_cdev->cdev_name);

qmi_send_exit:
	mutex_unlock(&tmd->mutex);
	return ret;
}

static int qmi_set_cur_or_min_state(struct qmi_cooling_device *qmi_cdev,
				 unsigned long state)
{
	int ret = 0;
	struct qmi_tmd_instance *tmd = qmi_cdev->tmd;

	if (!tmd)
		return -EINVAL;

	if (qmi_cdev->mtgn_state == state)
		return ret;

	/* save it and return if server exit */
	if (!qmi_cdev->connection_active) {
		qmi_cdev->mtgn_state = state;
		pr_debug("Pending request:%ld for %s\n", state,
				qmi_cdev->cdev_name);
		return ret;
	}

	/* It is best effort to save state even if QMI fail */
	ret = qmi_tmd_send_state_request(qmi_cdev, (uint8_t)state);

	qmi_cdev->mtgn_state = state;

	return ret;
}

static int qmi_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct qmi_cooling_device *qmi_cdev = cdev->devdata;

	if (!qmi_cdev)
		return -EINVAL;

	if (qmi_cdev->type == QMI_CDEV_MIN_LIMIT_TYPE)
		return 0;

	if (state > qmi_cdev->max_level)
		state = qmi_cdev->max_level;

	return qmi_set_cur_or_min_state(qmi_cdev, state);
}

static int qmi_set_min_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct qmi_cooling_device *qmi_cdev = cdev->devdata;

	if (!qmi_cdev)
		return -EINVAL;

	if (qmi_cdev->type == QMI_CDEV_MAX_LIMIT_TYPE)
		return 0;

	if (state > qmi_cdev->max_level)
		state = qmi_cdev->max_level;

	/* Convert state into QMI client expects for min state */
	state = qmi_cdev->max_level - state;

	return qmi_set_cur_or_min_state(qmi_cdev, state);
}

static int qmi_get_min_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct qmi_cooling_device *qmi_cdev = cdev->devdata;

	if (!qmi_cdev)
		return -EINVAL;

	if (qmi_cdev->type == QMI_CDEV_MAX_LIMIT_TYPE) {
		*state = 0;
		return 0;
	}
	*state = qmi_cdev->max_level - qmi_cdev->mtgn_state;

	return 0;
}

static struct thermal_cooling_device_ops qmi_device_ops = {
	.get_max_state = qmi_get_max_state,
	.get_cur_state = qmi_get_cur_state,
	.set_cur_state = qmi_set_cur_state,
	.set_min_state = qmi_set_min_state,
	.get_min_state = qmi_get_min_state,
};

static int qmi_register_cooling_device(struct qmi_cooling_device *qmi_cdev)
{
	qmi_cdev->cdev = thermal_of_cooling_device_register(
					qmi_cdev->np,
					qmi_cdev->cdev_name,
					qmi_cdev,
					&qmi_device_ops);
	if (IS_ERR(qmi_cdev->cdev)) {
		pr_err("Cooling register failed for %s, ret:%ld\n",
			qmi_cdev->cdev_name, PTR_ERR(qmi_cdev->cdev));
		return PTR_ERR(qmi_cdev->cdev);
	}
	pr_debug("Cooling register success for %s\n", qmi_cdev->cdev_name);

	return 0;
}

static int verify_devices_and_register(struct qmi_tmd_instance *tmd)
{
	struct tmd_get_mitigation_device_list_req_msg_v01 req;
	struct tmd_get_mitigation_device_list_resp_msg_v01 *tmd_resp;
	struct msg_desc req_desc, resp_desc;
	int ret = 0, i;

	memset(&req, 0, sizeof(req));
	/* size of tmd_resp is very high, use heap memory rather than stack */
	tmd_resp = kzalloc(sizeof(*tmd_resp), GFP_KERNEL);
	if (!tmd_resp)
		return -ENOMEM;

	req_desc.max_msg_len =
		TMD_GET_MITIGATION_DEVICE_LIST_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_TMD_GET_MITIGATION_DEVICE_LIST_REQ_V01;
	req_desc.ei_array = tmd_get_mitigation_device_list_req_msg_v01_ei;

	resp_desc.max_msg_len =
		TMD_GET_MITIGATION_DEVICE_LIST_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_TMD_GET_MITIGATION_DEVICE_LIST_RESP_V01;
	resp_desc.ei_array = tmd_get_mitigation_device_list_resp_msg_v01_ei;

	mutex_lock(&tmd->mutex);
	ret = qmi_send_req_wait(tmd->handle,
			&req_desc, &req, sizeof(req),
			&resp_desc, tmd_resp, sizeof(*tmd_resp),
			0);
	if (ret < 0) {
		pr_err("qmi get device list failed for inst_id:0x%x ret:%d\n",
			tmd->inst_id, ret);
		goto reg_exit;
	}

	if (tmd_resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = tmd_resp->resp.result;
		pr_err("Get device list NOT success for inst_id:0x%x ret:%d\n",
			tmd->inst_id, ret);
		goto reg_exit;
	}
	mutex_unlock(&tmd->mutex);

	for (i = 0; i < tmd_resp->mitigation_device_list_len; i++) {
		struct qmi_cooling_device *qmi_cdev = NULL;

		list_for_each_entry(qmi_cdev, &tmd->tmd_cdev_list,
					qmi_node) {
			struct tmd_mitigation_dev_list_type_v01 *device =
				&tmd_resp->mitigation_device_list[i];

			if ((strncasecmp(qmi_cdev->qmi_name,
				device->mitigation_dev_id.mitigation_dev_id,
				QMI_TMD_MITIGATION_DEV_ID_LENGTH_MAX_V01)))
				continue;

			qmi_cdev->connection_active = true;
			qmi_cdev->max_level = device->max_mitigation_level;
			/*
			 * It is better to set current state
			 * initially or during restart
			 */
			qmi_tmd_send_state_request(qmi_cdev,
						qmi_cdev->mtgn_state);
			if (!qmi_cdev->cdev)
				ret = qmi_register_cooling_device(qmi_cdev);
			break;
		}
	}

	kfree(tmd_resp);
	return ret;

reg_exit:
	mutex_unlock(&tmd->mutex);
	kfree(tmd_resp);

	return ret;
}

static void qmi_tmd_rcv_msg(struct work_struct *work)
{
	int rc;
	struct qmi_tmd_instance *tmd = container_of(work,
						struct qmi_tmd_instance,
						work_rcv_msg);

	do {
		pr_debug("Notified about a Receive Event\n");
	} while ((rc = qmi_recv_msg(tmd->handle)) == 0);

	if (rc != -ENOMSG)
		pr_err("Error receiving message for SVC:0x%x, ret:%d\n",
			tmd->inst_id, rc);
}

static void qmi_tmd_clnt_notify(struct qmi_handle *handle,
		enum qmi_event_type event, void *priv_data)
{
	struct qmi_tmd_instance *tmd =
		(struct qmi_tmd_instance *)priv_data;

	if (!tmd) {
		pr_debug("tmd is NULL\n");
		return;
	}

	switch (event) {
	case QMI_RECV_MSG:
		queue_work(qmi_tmd_wq, &tmd->work_rcv_msg);
		break;
	default:
		break;
	}
}

static void qmi_tmd_svc_arrive(struct work_struct *work)
{
	int ret = 0;
	struct qmi_tmd_instance *tmd = container_of(work,
						struct qmi_tmd_instance,
						work_svc_arrive);

	mutex_lock(&tmd->mutex);
	tmd->handle = qmi_handle_create(qmi_tmd_clnt_notify, tmd);
	if (!tmd->handle) {
		pr_err("QMI TMD client handle alloc failed for 0x%x\n",
			tmd->inst_id);
		goto arrive_exit;
	}

	ret = qmi_connect_to_service(tmd->handle, TMD_SERVICE_ID_V01,
				   TMD_SERVICE_VERS_V01,
				   tmd->inst_id);
	if (ret < 0) {
		pr_err("Could not connect handle to service for 0x%x, ret:%d\n",
			tmd->inst_id, ret);
		qmi_handle_destroy(tmd->handle);
		tmd->handle = NULL;
		goto arrive_exit;
	}
	mutex_unlock(&tmd->mutex);

	verify_devices_and_register(tmd);

	return;

arrive_exit:
	mutex_unlock(&tmd->mutex);
}

static void qmi_tmd_svc_exit(struct work_struct *work)
{
	struct qmi_tmd_instance *tmd = container_of(work,
						struct qmi_tmd_instance,
						work_svc_exit);
	struct qmi_cooling_device *qmi_cdev;

	mutex_lock(&tmd->mutex);
	qmi_handle_destroy(tmd->handle);
	tmd->handle = NULL;

	list_for_each_entry(qmi_cdev, &tmd->tmd_cdev_list, qmi_node)
		qmi_cdev->connection_active = false;

	mutex_unlock(&tmd->mutex);
}

static int qmi_tmd_svc_event_notify(struct notifier_block *this,
				    unsigned long event,
				    void *data)
{
	struct qmi_tmd_instance *tmd = container_of(this,
						struct qmi_tmd_instance,
						nb);

	if (!tmd) {
		pr_debug("tmd is NULL\n");
		return -EINVAL;
	}

	switch (event) {
	case QMI_SERVER_ARRIVE:
		schedule_work(&tmd->work_svc_arrive);
		break;
	case QMI_SERVER_EXIT:
		schedule_work(&tmd->work_svc_exit);
		break;
	default:
		break;
	}
	return 0;
}

static void qmi_tmd_cleanup(void)
{
	int idx = 0;
	struct qmi_tmd_instance *tmd = tmd_instances;
	struct qmi_cooling_device *qmi_cdev, *c_next;

	for (; idx < tmd_inst_cnt; idx++) {
		mutex_lock(&tmd[idx].mutex);
		list_for_each_entry_safe(qmi_cdev, c_next,
				&tmd[idx].tmd_cdev_list, qmi_node) {
			if (qmi_cdev->cdev)
				thermal_cooling_device_unregister(
					qmi_cdev->cdev);

			list_del(&qmi_cdev->qmi_node);
		}
		if (tmd[idx].handle)
			qmi_handle_destroy(tmd[idx].handle);

		if (tmd[idx].nb.notifier_call)
			qmi_svc_event_notifier_unregister(TMD_SERVICE_ID_V01,
						TMD_SERVICE_VERS_V01,
						tmd[idx].inst_id,
						&tmd[idx].nb);
		mutex_unlock(&tmd[idx].mutex);
	}

	if (qmi_tmd_wq) {
		destroy_workqueue(qmi_tmd_wq);
		qmi_tmd_wq = NULL;
	}
}

static int of_get_qmi_tmd_platform_data(struct device *dev)
{
	int ret = 0, idx = 0, i = 0, subsys_cnt = 0;
	struct device_node *np = dev->of_node;
	struct device_node *subsys_np, *cdev_np;
	struct qmi_tmd_instance *tmd;
	struct qmi_cooling_device *qmi_cdev;

	subsys_cnt = of_get_available_child_count(np);
	if (!subsys_cnt) {
		dev_err(dev, "No child node to process\n");
		return -EFAULT;
	}

	tmd = devm_kcalloc(dev, subsys_cnt, sizeof(*tmd), GFP_KERNEL);
	if (!tmd)
		return -ENOMEM;

	for_each_available_child_of_node(np, subsys_np) {
		if (idx >= subsys_cnt)
			break;

		ret = of_property_read_u32(subsys_np, "qcom,instance-id",
				&tmd[idx].inst_id);
		if (ret) {
			dev_err(dev, "error reading qcom,insance-id. ret:%d\n",
				ret);
			return ret;
		}

		tmd[idx].dev = dev;
		mutex_init(&tmd[idx].mutex);
		INIT_LIST_HEAD(&tmd[idx].tmd_cdev_list);

		for_each_available_child_of_node(subsys_np, cdev_np) {
			const char *qmi_name;

			qmi_cdev = devm_kzalloc(dev, sizeof(*qmi_cdev),
					GFP_KERNEL);
			if (!qmi_cdev) {
				ret = -ENOMEM;
				return ret;
			}

			strlcpy(qmi_cdev->cdev_name, cdev_np->name,
				THERMAL_NAME_LENGTH);

			if (!of_property_read_string(cdev_np,
					"qcom,qmi-dev-name",
					&qmi_name)) {
				strlcpy(qmi_cdev->qmi_name, qmi_name,
						QMI_CLIENT_NAME_LENGTH);
			} else {
				dev_err(dev, "Fail to parse dev name for %s\n",
					cdev_np->name);
				break;
			}
			/* Check for supported qmi dev*/
			for (i = 0; i < ARRAY_SIZE(device_clients); i++) {
				if (strcmp(device_clients[i].dev_name,
					qmi_cdev->qmi_name) == 0)
					break;
			}

			if (i >= ARRAY_SIZE(device_clients)) {
				dev_err(dev, "Not supported dev name for %s\n",
					cdev_np->name);
				break;
			}
			qmi_cdev->type = device_clients[i].type;
			qmi_cdev->tmd = &tmd[idx];
			qmi_cdev->np = cdev_np;
			qmi_cdev->mtgn_state = 0;
			list_add(&qmi_cdev->qmi_node, &tmd[idx].tmd_cdev_list);
		}
		idx++;
	}
	tmd_instances = tmd;
	tmd_inst_cnt = subsys_cnt;

	return 0;
}

static int qmi_device_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0, idx = 0;

	ret = of_get_qmi_tmd_platform_data(dev);
	if (ret)
		goto probe_err;

	if (!tmd_instances || !tmd_inst_cnt) {
		dev_err(dev, "Empty tmd instances\n");
		return -EINVAL;
	}

	qmi_tmd_wq = create_singlethread_workqueue("qmi_tmd_wq");
	if (!qmi_tmd_wq) {
		dev_err(dev, "Failed to create single thread workqueue\n");
		ret = -EFAULT;
		goto probe_err;
	}

	for (; idx < tmd_inst_cnt; idx++) {
		struct qmi_tmd_instance *tmd = &tmd_instances[idx];

		if (list_empty(&tmd->tmd_cdev_list))
			continue;

		tmd->nb.notifier_call = qmi_tmd_svc_event_notify;
		INIT_WORK(&tmd->work_svc_arrive, qmi_tmd_svc_arrive);
		INIT_WORK(&tmd->work_svc_exit, qmi_tmd_svc_exit);
		INIT_WORK(&tmd->work_rcv_msg, qmi_tmd_rcv_msg);

		ret = qmi_svc_event_notifier_register(TMD_SERVICE_ID_V01,
						TMD_SERVICE_VERS_V01,
						tmd->inst_id,
						&tmd->nb);
		if (ret < 0) {
			dev_err(dev, "QMI register failed for 0x%x, ret:%d\n",
				tmd->inst_id, ret);
			goto probe_err;
		}
	}

	return 0;

probe_err:
	qmi_tmd_cleanup();
	return ret;
}

static int qmi_device_remove(struct platform_device *pdev)
{
	qmi_tmd_cleanup();

	return 0;
}

static const struct of_device_id qmi_device_match[] = {
	{.compatible = "qcom,qmi_cooling_devices"},
	{}
};

static struct platform_driver qmi_device_driver = {
	.probe          = qmi_device_probe,
	.remove         = qmi_device_remove,
	.driver         = {
		.name   = "QMI_CDEV_DRIVER",
		.owner  = THIS_MODULE,
		.of_match_table = qmi_device_match,
	},
};

static int __init qmi_device_init(void)
{
	return platform_driver_register(&qmi_device_driver);
}
module_init(qmi_device_init);

static void __exit qmi_device_exit(void)
{
	platform_driver_unregister(&qmi_device_driver);
}
module_exit(qmi_device_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QTI QMI cooling device driver");
