/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/qmi_encdec.h>

#include <asm/uaccess.h>

#include <mach/msm_qmi_interface.h>

#include "kernel_test_service_v01.h"

#define TEST_SERVICE_SVC_ID 0x0000000f
#define TEST_SERVICE_INS_ID 1

static int test_rep_cnt = 10;
module_param_named(rep_cnt, test_rep_cnt, int, S_IRUGO | S_IWUSR | S_IWGRP);

static int test_data_sz = 50;
module_param_named(data_sz, test_data_sz, int, S_IRUGO | S_IWUSR | S_IWGRP);

static int test_clnt_debug_mask;
module_param_named(debug_mask, test_clnt_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#define D(x...) do { \
	if (test_clnt_debug_mask) \
		pr_debug(x); \
} while (0)

/* Variable to initiate the test through debugfs interface */
static struct dentry *test_dent;

/* Test client port for IPC Router */
static struct qmi_handle *test_clnt;
static int test_clnt_reset;

/* Reader thread to receive responses & indications */
static void test_clnt_recv_msg(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_recv_msg, test_clnt_recv_msg);
static void test_clnt_svc_arrive(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_svc_arrive, test_clnt_svc_arrive);
static void test_clnt_svc_exit(struct work_struct *work);
static DECLARE_DELAYED_WORK(work_svc_exit, test_clnt_svc_exit);
static struct workqueue_struct *test_clnt_workqueue;

/* Variable to hold the test result */
static int test_res;

static int test_qmi_ping_pong_send_sync_msg(void)
{
	struct test_ping_req_msg_v01 req;
	struct test_ping_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;
	int rc;

	memcpy(req.ping, "ping", sizeof(req.ping));
	req.client_name_valid = 0;

	req_desc.max_msg_len = TEST_PING_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = TEST_PING_REQ_MSG_ID_V01;
	req_desc.ei_array = test_ping_req_msg_v01_ei;

	resp_desc.max_msg_len = TEST_PING_REQ_MAX_MSG_LEN_V01;
	resp_desc.msg_id = TEST_PING_REQ_MSG_ID_V01;
	resp_desc.ei_array = test_ping_resp_msg_v01_ei;

	rc = qmi_send_req_wait(test_clnt, &req_desc, &req, sizeof(req),
			       &resp_desc, &resp, sizeof(resp), 0);
	if (rc < 0) {
		pr_err("%s: send req failed %d\n", __func__, rc);
		return rc;
	}

	D("%s: Received %s response\n", __func__, resp.pong);
	return rc;
}

static int test_qmi_data_send_sync_msg(unsigned int data_len)
{
	struct test_data_req_msg_v01 *req;
	struct test_data_resp_msg_v01 *resp;
	struct msg_desc req_desc, resp_desc;
	int rc, i;

	req = kzalloc(sizeof(struct test_data_req_msg_v01), GFP_KERNEL);
	if (!req) {
		pr_err("%s: Data req msg alloc failed\n", __func__);
		return -ENOMEM;
	}

	resp = kzalloc(sizeof(struct test_data_resp_msg_v01), GFP_KERNEL);
	if (!resp) {
		pr_err("%s: Data resp msg alloc failed\n", __func__);
		kfree(req);
		return -ENOMEM;
	}

	req->data_len = data_len;
	for (i = 0; i < data_len; i = i + sizeof(int))
		memcpy(req->data + i, (uint8_t *)&i, sizeof(int));
	req->client_name_valid = 0;

	req_desc.max_msg_len = TEST_DATA_REQ_MAX_MSG_LEN_V01;
	req_desc.msg_id = TEST_DATA_REQ_MSG_ID_V01;
	req_desc.ei_array = test_data_req_msg_v01_ei;

	resp_desc.max_msg_len = TEST_DATA_REQ_MAX_MSG_LEN_V01;
	resp_desc.msg_id = TEST_DATA_REQ_MSG_ID_V01;
	resp_desc.ei_array = test_data_resp_msg_v01_ei;

	rc = qmi_send_req_wait(test_clnt, &req_desc, req, sizeof(*req),
			       &resp_desc, resp, sizeof(*resp), 0);
	if (rc < 0) {
		pr_err("%s: send req failed\n", __func__);
		goto data_send_err;
	}

	D("%s: data_valid %d\n", __func__, resp->data_valid);
	D("%s: data_len %d\n", __func__, resp->data_len);
data_send_err:
	kfree(resp);
	kfree(req);
	return rc;
}

static void test_clnt_recv_msg(struct work_struct *work)
{
	int rc;

	rc = qmi_recv_msg(test_clnt);
	if (rc < 0)
		pr_err("%s: Error receiving message\n", __func__);
}

static void test_clnt_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{
	switch (event) {
	case QMI_RECV_MSG:
		queue_delayed_work(test_clnt_workqueue,
				   &work_recv_msg, 0);
		break;
	default:
		break;
	}
}

static void test_clnt_svc_arrive(struct work_struct *work)
{
	int rc;

	D("%s begins\n", __func__);

	/* Create a Local client port for QMI communication */
	test_clnt = qmi_handle_create(test_clnt_notify, NULL);
	if (!test_clnt) {
		pr_err("%s: QMI client handle alloc failed\n", __func__);
		return;
	}

	D("%s: Lookup server name\n", __func__);
	rc = qmi_connect_to_service(test_clnt, TEST_SERVICE_SVC_ID,
				    TEST_SERVICE_INS_ID);
	if (rc < 0) {
		pr_err("%s: Server not found\n", __func__);
		qmi_handle_destroy(test_clnt);
		test_clnt = NULL;
		return;
	}
	test_clnt_reset = 0;
	D("%s complete\n", __func__);
}

static void test_clnt_svc_exit(struct work_struct *work)
{
	D("%s begins\n", __func__);

	qmi_handle_destroy(test_clnt);
	test_clnt_reset = 1;
	test_clnt = NULL;

	D("%s complete\n", __func__);
}

static int test_clnt_svc_event_notify(struct notifier_block *this,
				      unsigned long code,
				      void *_cmd)
{
	D("%s: event %ld\n", __func__, code);
	switch (code) {
	case QMI_SERVER_ARRIVE:
		queue_delayed_work(test_clnt_workqueue,
				   &work_svc_arrive, 0);
		break;
	case QMI_SERVER_EXIT:
		queue_delayed_work(test_clnt_workqueue,
				   &work_svc_exit, 0);
		break;
	default:
		break;
	}
	return 0;
}

static int test_qmi_open(struct inode *ip, struct file *fp)
{
	if (!test_clnt) {
		pr_err("%s Test client is not initialized\n", __func__);
		return -ENODEV;
	}
	return 0;
}

static ssize_t test_qmi_read(struct file *fp, char __user *buf,
		size_t count, loff_t *pos)
{
	char _buf[16];
	snprintf(_buf, sizeof(_buf), "%d\n", test_res);
	test_res = 0;
	return simple_read_from_buffer(buf, count, pos,
				       _buf, strnlen(_buf, 16));
}

static int test_qmi_release(struct inode *ip, struct file *fp)
{
	return 0;
}

static ssize_t test_qmi_write(struct file *fp, const char __user *buf,
			size_t count, loff_t *pos)
{
	unsigned char cmd[64];
	int len;
	int i;

	if (count < 1)
		return 0;

	len = min(count, (sizeof(cmd) - 1));

	if (copy_from_user(cmd, buf, len))
		return -EFAULT;

	cmd[len] = 0;
	if (cmd[len-1] == '\n') {
		cmd[len-1] = 0;
		len--;
	}

	if (!strncmp(cmd, "ping_pong", sizeof(cmd))) {
		for (i = 0; i < test_rep_cnt; i++) {
			test_res = test_qmi_ping_pong_send_sync_msg();
			if (test_res == -ENETRESET || test_clnt_reset) {
				do {
					msleep(50);
				} while (test_clnt_reset);
			}
		}
	} else if (!strncmp(cmd, "data", sizeof(cmd))) {
		for (i = 0; i < test_rep_cnt; i++) {
			test_res = test_qmi_data_send_sync_msg(test_data_sz);
			if (test_res == -ENETRESET || test_clnt_reset) {
				do {
					msleep(50);
				} while (test_clnt_reset);
			}
		}
	} else {
		test_res = -EINVAL;
	}
	return count;
}

static struct notifier_block test_clnt_nb = {
	.notifier_call = test_clnt_svc_event_notify,
};

static const struct file_operations debug_ops = {
	.owner = THIS_MODULE,
	.open = test_qmi_open,
	.read = test_qmi_read,
	.write = test_qmi_write,
	.release = test_qmi_release,
};

static int __init test_qmi_init(void)
{
	int rc;

	test_clnt_workqueue = create_singlethread_workqueue("test_clnt");
	if (!test_clnt_workqueue)
		return -EFAULT;

	rc = qmi_svc_event_notifier_register(TEST_SERVICE_SVC_ID,
				TEST_SERVICE_INS_ID, &test_clnt_nb);
	if (rc < 0) {
		pr_err("%s: notifier register failed\n", __func__);
		destroy_workqueue(test_clnt_workqueue);
		return rc;
	}

	test_dent = debugfs_create_file("test_qmi_client", 0444, 0,
					 NULL, &debug_ops);
	if (IS_ERR(test_dent)) {
		pr_err("%s: unable to create debugfs %ld\n",
			__func__, IS_ERR(test_dent));
		test_dent = NULL;
		qmi_svc_event_notifier_unregister(TEST_SERVICE_SVC_ID,
					TEST_SERVICE_INS_ID, &test_clnt_nb);
		destroy_workqueue(test_clnt_workqueue);
		return -EFAULT;
	}

	return 0;
}

static void __exit test_qmi_exit(void)
{
	qmi_svc_event_notifier_unregister(TEST_SERVICE_SVC_ID,
					TEST_SERVICE_INS_ID, &test_clnt_nb);
	destroy_workqueue(test_clnt_workqueue);
	debugfs_remove(test_dent);
}

module_init(test_qmi_init);
module_exit(test_qmi_exit);

MODULE_DESCRIPTION("TEST QMI Client Driver");
MODULE_LICENSE("GPL v2");
