/*
 * Sample QRTR client driver
 *
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017 Linaro Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
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
#include <linux/qrtr.h>
#include <linux/net.h>
#include <linux/completion.h>
#include <linux/idr.h>
#include <linux/string.h>
#include <net/sock.h>
#include <linux/soc/qcom/qmi.h>

#define PING_REQ1_TLV_TYPE		0x1
#define PING_RESP1_TLV_TYPE		0x2
#define PING_OPT1_TLV_TYPE		0x10
#define PING_OPT2_TLV_TYPE		0x11

#define DATA_REQ1_TLV_TYPE		0x1
#define DATA_RESP1_TLV_TYPE		0x2
#define DATA_OPT1_TLV_TYPE		0x10
#define DATA_OPT2_TLV_TYPE		0x11

#define TEST_MED_DATA_SIZE_V01		8192
#define TEST_MAX_NAME_SIZE_V01		255

#define TEST_PING_REQ_MSG_ID_V01	0x20
#define TEST_DATA_REQ_MSG_ID_V01	0x21

#define TEST_PING_REQ_MAX_MSG_LEN_V01	266
#define TEST_DATA_REQ_MAX_MSG_LEN_V01	8456

struct test_name_type_v01 {
	uint32_t name_len;
	char name[TEST_MAX_NAME_SIZE_V01];
};

static struct qmi_elem_info test_name_type_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size      = sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct test_name_type_v01,
					   name_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = TEST_MAX_NAME_SIZE_V01,
		.elem_size      = sizeof(char),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
		.offset         = offsetof(struct test_name_type_v01,
					   name),
	},
	{}
};

struct test_ping_req_msg_v01 {
	char ping[4];

	uint8_t client_name_valid;
	struct test_name_type_v01 client_name;
};

struct qmi_elem_info test_ping_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 4,
		.elem_size      = sizeof(char),
		.is_array       = STATIC_ARRAY,
		.tlv_type       = PING_REQ1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_req_msg_v01,
					   ping),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_req_msg_v01,
					   client_name_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct test_name_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_req_msg_v01,
					   client_name),
		.ei_array       = test_name_type_v01_ei,
	},
	{}
};

struct test_ping_resp_msg_v01 {
	struct qmi_response_type_v01 resp;

	uint8_t pong_valid;
	char pong[4];

	uint8_t service_name_valid;
	struct test_name_type_v01 service_name;
};

struct qmi_elem_info test_ping_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_RESP1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_resp_msg_v01,
					   resp),
		.ei_array       = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_resp_msg_v01,
					   pong_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 4,
		.elem_size      = sizeof(char),
		.is_array       = STATIC_ARRAY,
		.tlv_type       = PING_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_resp_msg_v01,
					   pong),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_OPT2_TLV_TYPE,
		.offset         = offsetof(struct test_ping_resp_msg_v01,
					   service_name_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct test_name_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_OPT2_TLV_TYPE,
		.offset         = offsetof(struct test_ping_resp_msg_v01,
					   service_name),
		.ei_array       = test_name_type_v01_ei,
	},
	{}
};

struct test_data_req_msg_v01 {
	uint32_t data_len;
	uint8_t data[TEST_MED_DATA_SIZE_V01];

	uint8_t client_name_valid;
	struct test_name_type_v01 client_name;
};

struct qmi_elem_info test_data_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_REQ1_TLV_TYPE,
		.offset         = offsetof(struct test_data_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = TEST_MED_DATA_SIZE_V01,
		.elem_size      = sizeof(uint8_t),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = DATA_REQ1_TLV_TYPE,
		.offset         = offsetof(struct test_data_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_data_req_msg_v01,
					   client_name_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct test_name_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_data_req_msg_v01,
					   client_name),
		.ei_array       = test_name_type_v01_ei,
	},
	{}
};

struct test_data_resp_msg_v01 {
	struct qmi_response_type_v01 resp;

	uint8_t data_valid;
	uint32_t data_len;
	uint8_t data[TEST_MED_DATA_SIZE_V01];

	uint8_t service_name_valid;
	struct test_name_type_v01 service_name;
};

struct qmi_elem_info test_data_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_RESP1_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   resp),
		.ei_array       = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = TEST_MED_DATA_SIZE_V01,
		.elem_size      = sizeof(uint8_t),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = DATA_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT2_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   service_name_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct test_name_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT2_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   service_name),
		.ei_array       = test_name_type_v01_ei,
	},
	{}
};

/*
 * ping_pong_store() - ping_pong attribute store handler
 * @dev:	sample device context
 * @attr:	the ping_pong attribute
 * @buf:	write buffer
 * @count:	length of @buf
 *
 * Returns @count, or negative errno on failure.
 *
 * This function allows user space to send out a ping_pong QMI encoded message
 * to the associated remote test service and will return with the result of the
 * transaction. It serves as an example of how to provide a custom response
 * handler.
 */
static ssize_t ping_pong_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct qmi_handle *qmi = dev_get_drvdata(dev);
	struct test_ping_req_msg_v01 req = {0};
	struct qmi_txn txn;
	int ret;

	memcpy(req.ping, "ping", sizeof(req.ping));

	ret = qmi_txn_init(qmi, &txn, NULL, NULL);
	if (ret < 0)
		return ret;

	ret = qmi_send_message(qmi, NULL, &txn,
			       QMI_REQUEST,
			       TEST_PING_REQ_MSG_ID_V01,
			       TEST_PING_REQ_MAX_MSG_LEN_V01,
			       test_ping_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		return ret;
	}

	ret = qmi_txn_wait(&txn, 5 * HZ);
	if (ret < 0)
		count = ret;

	return count;
}
static DEVICE_ATTR_WO(ping_pong);

static void ping_pong_cb(struct qmi_handle *qmi, struct sockaddr_qrtr *sq,
			 struct qmi_txn *txn, const void *data)
{
	const struct test_ping_resp_msg_v01 *resp = data;

	if (!txn) {
		pr_err("spurious ping response\n");
		return;
	}

	if (resp->resp.result == QMI_RESULT_FAILURE_V01)
		txn->result = -ENXIO;
	else if (!resp->pong_valid || memcmp(resp->pong, "pong", 4))
		txn->result = -EINVAL;

	complete(&txn->completion);
}

/*
 * data_store() - data attribute store handler
 * @dev:	sample device context
 * @attr:	the data attribute
 * @buf:	buffer with message to encode
 * @count:	length of @buf
 *
 * Returns @count, or negative errno on failure.
 *
 * This function allows user space to send out a data QMI encoded message to
 * the associated remote test service and will return with the result of the
 * transaction. It serves as an example of how to have the QMI helpers decode a
 * transaction response into a provided object automatically.
 */
static ssize_t data_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct qmi_handle *qmi = dev_get_drvdata(dev);
	struct test_data_resp_msg_v01 *resp;
	struct test_data_req_msg_v01 *req;
	struct qmi_txn txn;
	int ret;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->data_len = min_t(size_t, sizeof(req->data), count);
	memcpy(req->data, buf, req->data_len);

	ret = qmi_txn_init(qmi, &txn, test_data_resp_msg_v01_ei, resp);
	if (ret < 0) {
		count = ret;
		goto out;
	}

	ret = qmi_send_message(qmi, NULL, &txn,
			       QMI_REQUEST,
			       TEST_DATA_REQ_MSG_ID_V01,
			       TEST_DATA_REQ_MAX_MSG_LEN_V01,
			       test_data_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		count = ret;
		goto out;
	}

	ret = qmi_txn_wait(&txn, 5 * HZ);
	if (ret < 0) {
		count = ret;
	} else if (!resp->data_valid ||
		   resp->data_len != req->data_len ||
		   memcmp(resp->data, req->data, req->data_len)) {
		pr_err("response data doesn't match expectation\n");
		count = -EINVAL;
	}

out:
	kfree(resp);
	kfree(req);

	return count;
}
static DEVICE_ATTR_WO(data);

static struct attribute *qrtr_dev_attrs[] = {
	&dev_attr_ping_pong.attr,
	&dev_attr_data.attr,
	NULL
};
ATTRIBUTE_GROUPS(qrtr_dev);

static struct qmi_msg_handler qrtr_sample_handlers[] = {
	{
		.type = QMI_RESPONSE,
		.msg_id = TEST_PING_REQ_MSG_ID_V01,
		.ei = test_ping_resp_msg_v01_ei,
		.decoded_size = sizeof(struct test_ping_req_msg_v01),
		.fn = ping_pong_cb
	},
	{}
};

static int qrtr_sample_probe(struct platform_device *pdev)
{
	struct qrtr_handle *qrtr;
	struct qmi_handle *qmi;
	struct sockaddr_qrtr *sq;
	int ret;

	qmi = devm_kzalloc(&pdev->dev, sizeof(*qmi), GFP_KERNEL);
	if (!qmi)
		return -ENOMEM;

	qrtr = &qmi->qrtr;

	ret = qmi_client_init(qmi, TEST_DATA_REQ_MAX_MSG_LEN_V01,
			      qrtr_sample_handlers);
	if (ret < 0)
		return ret;

	sq = dev_get_platdata(&pdev->dev);
	ret = kernel_connect(qrtr->sock, (struct sockaddr *)sq,
			     sizeof(*sq), 0);
	if (ret < 0) {
		pr_err("failed to connect to remote service port\n");
		qmi_client_release(qmi);
		return ret;
	}

	platform_set_drvdata(pdev, qmi);

	return 0;
}

static int qrtr_sample_remove(struct platform_device *pdev)
{
	struct qmi_handle *qmi = platform_get_drvdata(pdev);

	qmi_client_release(qmi);

	return 0;
}

static struct platform_driver qrtr_sample_driver = {
	.probe = qrtr_sample_probe,
	.remove = qrtr_sample_remove,
	.driver = {
		.name = "qrtr_sample_client",
	},
};

static int qrtr_sample_new_server(struct qrtr_handle *qrtr,
				  struct qrtr_service *service)
{
	struct platform_device *pdev;
	struct sockaddr_qrtr sq = { AF_QIPCRTR, service->node, service->port };
	char name[32];
	int ret;

	snprintf(name, sizeof(name), "qrtr_sample_client@%d:%d",
		 service->node, service->port);

	pdev = platform_device_alloc(name, PLATFORM_DEVID_NONE);
	if (!pdev)
		return -ENOMEM;

	ret = platform_device_add_data(pdev, &sq, sizeof(sq));
	if (ret)
		goto err_put_device;

	pdev->dev.groups = qrtr_dev_groups;
	pdev->driver_override = (char *)qrtr_sample_driver.driver.name;
	ret = platform_device_add(pdev);
	if (ret)
		goto err_put_device;

	service->cookie = pdev;

	return 0;

err_put_device:
	platform_device_put(pdev);

	return ret;
}

static void qrtr_sample_del_server(struct qrtr_handle *qrtr,
				   struct qrtr_service *service)
{
	struct platform_device *pdev = service->cookie;

	platform_device_unregister(pdev);
}

static struct qrtr_handle lookup_client;

static struct qrtr_handle_ops lookup_ops;

static void qrtr_sample_net_reset_work(struct work_struct *work)
{
	int ret;

	qrtr_client_release(&lookup_client);

	ret = qrtr_client_init(&lookup_client, 0, &lookup_ops);
	if (ret < 0)
		return;

	qrtr_client_new_lookup(&lookup_client, 15, 0);
}
static DECLARE_WORK(net_reset_work, qrtr_sample_net_reset_work);

static void qrtr_sample_net_reset(struct qrtr_handle *qrtr)
{
	schedule_work(&net_reset_work);
}

static struct qrtr_handle_ops lookup_ops = {
	.new_server = qrtr_sample_new_server,
	.del_server = qrtr_sample_del_server,
	.net_reset = qrtr_sample_net_reset,
};

static int qrtr_sample_init(void)
{
	int ret;

	ret = platform_driver_register(&qrtr_sample_driver);
	if (ret)
		return ret;

	ret = qrtr_client_init(&lookup_client, 0, &lookup_ops);
	if (ret < 0)
		goto err_unregister_driver;

	qrtr_client_new_lookup(&lookup_client, 15, 0);

	return 0;

err_unregister_driver:
	platform_driver_unregister(&qrtr_sample_driver);

	return ret;
}

static void qrtr_sample_exit(void)
{
	qrtr_client_release(&lookup_client);

	platform_driver_unregister(&qrtr_sample_driver);
}

module_init(qrtr_sample_init);
module_exit(qrtr_sample_exit);

MODULE_DESCRIPTION("Sample QRTR client driver");
MODULE_LICENSE("GPL v2");
