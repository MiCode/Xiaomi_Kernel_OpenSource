/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>

#include "tz_dcih.h"
#include "tz_dcih_test.h"
#define IMSG_TAG "[tz_dcih_test]"
#include <imsg_log.h>
#include <isee_kernel_api.h>

static struct task_struct *test_notify_thread;
bool start_notify_test;
static struct task_struct *test_wait_notify_thread;
bool start_wait_notify_test;
static DEFINE_MUTEX(test_status_mutex);

static DECLARE_COMPLETION(thread_start);
static DECLARE_COMPLETION(thread_finish);

static int dci_greeting_test(uint32_t driver_id)
{
	struct dci_message *msg;
	uint32_t tmp_value;
	int ret;
	int num_item = 0;

	msg = (struct dci_message *)tz_get_share_buffer(driver_id);

	memset(msg, 0, sizeof(struct dci_message));

	msg->cmd = GREETING;
	snprintf(msg->req_data, sizeof(msg->req_data),
				"Say Hello to 0x%x", driver_id);

	ret = tz_notify_driver(driver_id);
	if (ret < 0) {
		IMSG_ERROR("failed to notify secure driver, ret %d\n", ret);
		goto exit;
	}

	num_item = sscanf(msg->res_data, "Hello 0x%x", &tmp_value);
	if (num_item != 1 || tmp_value != driver_id) {
		IMSG_ERROR("return unexpected data, res_data [%s]\n",
								msg->res_data);
		ret = -EINVAL;
	}

exit:
	return ret;
}

static int dci_bit_op_not_test(uint32_t driver_id)
{
	struct dci_message *msg;
	int ret;
	int i;

	msg = (struct dci_message *)tz_get_share_buffer(driver_id);

	memset(msg, 0, sizeof(struct dci_message));

	msg->cmd = BIT_OP_NOT;
	for (i = 0; i < MAX_BUF_SIZE; i++)
		msg->req_data[i] = (uint8_t)(i & 0xff);

	ret = tz_notify_driver(driver_id);
	if (ret < 0) {
		IMSG_ERROR("failed to notify secure driver, ret %d\n", ret);
		goto exit;
	}

	for (i = 0; i < MAX_BUF_SIZE; i++) {
		if (msg->res_data[i] != (uint8_t)(~msg->req_data[i])) {
			IMSG_ERROR("return unexpected data:\n");
			IMSG_ERROR("res_data[%d] expect 0x%02x actual 0x%02x\n",
					i, (uint8_t)(~msg->req_data[i]),
					msg->res_data[i]);
			ret = -EINVAL;
			break;
		}
	}

exit:
	return ret;
}

static int notify_driver_test(void *data)
{
	uint32_t driver_id = (uintptr_t)data;
	int ret;

	IMSG_DEBUG("starting notify test thread, driver_id 0x%x\n", driver_id);

	allow_signal(SIGTERM);

	ret = tz_create_share_buffer(driver_id, MAX_DCIH_BUF_SIZE);
	if (ret < 0) {
		IMSG_ERROR("failed to create dci share buffer\n");
		goto exit;
	}

	ret = dci_greeting_test(driver_id);
	if (ret)
		goto exit;

	ret = dci_bit_op_not_test(driver_id);

exit:
	tz_free_share_buffer(driver_id);

	complete(&thread_finish);

	IMSG_DEBUG("notify test is finished, wait for terminate signal...\n");
	while (!kthread_should_stop())
		msleep(50);

	IMSG_DEBUG("Got terminate signal, going to stop thread\n");
	return ret;
}

static int process_dci_msg(uint32_t driver_id)
{
	struct dci_message *msg = NULL;
	uint32_t tmp_value;
	int i;
	int num_item;
	int ret = 0;

	msg = (struct dci_message *)tz_get_share_buffer(driver_id);

	IMSG_DEBUG("cmd %d\n", msg->cmd);

	switch (msg->cmd) {
	case GREETING:
		num_item = sscanf(msg->req_data, "Say Hello to 0x%x",
					&tmp_value);
		if (num_item != 1) {
			IMSG_ERROR("no item found in sscanf\n");
			ret = -EINVAL;
			break;
		}
		snprintf(msg->res_data, sizeof(msg->res_data),
						"Hello 0x%x", tmp_value);
		break;
	case BIT_OP_NOT:
		for (i = 0; i < MAX_BUF_SIZE; i++)
			msg->res_data[i] = (uint8_t)(~msg->req_data[i]);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int wait_driver_notify_test(void *data)
{
	uint32_t driver_id = (uintptr_t)data;
	int ret;

	IMSG_DEBUG("starting wait notify test thread, driver_id 0x%x\n",
			driver_id);

	allow_signal(SIGTERM);

	ret = tz_create_share_buffer(driver_id, MAX_DCIH_BUF_SIZE);
	if (ret < 0) {
		IMSG_ERROR("failed to create dci share buffer\n");
		complete(&thread_start);
		goto exit;
	}

	complete(&thread_start);

	while (!kthread_should_stop()) {
		ret = tz_wait_for_notification(driver_id);
		if (ret < 0) {
			if (ret == -ERESTARTSYS) {
				IMSG_INFO("waiting was interrupted\n");
				ret = 0;
			} else
				IMSG_ERROR("failed to waiting, ret %d\n", ret);
			break;
		}

		ret = process_dci_msg(driver_id);
		if (ret < 0)
			IMSG_ERROR("failed to process dci message, %d\n", ret);

		ret = tz_notify_driver(driver_id);
		if (ret < 0) {
			IMSG_ERROR("failed to notify secure driver, %d\n", ret);
			break;
		}
	}

exit:
	tz_free_share_buffer(driver_id);

	IMSG_DEBUG("Got terminate signal, going to stop thread\n");
	return ret;
}

void start_dcih_notify_test(uint32_t driver_id)
{
	uintptr_t data = driver_id;

	mutex_lock(&test_status_mutex);

	if (start_notify_test) {
		IMSG_ERROR("test thread is still running\n");
		goto exit;
	}

	start_notify_test = true;

	test_notify_thread = kthread_run(notify_driver_test,
				(void *)data, "dcih_notify_test_thread");
	if (IS_ERR(test_notify_thread)) {
		IMSG_ERROR("failed to create dcih test notify thread\n");
		test_notify_thread = NULL;
		start_notify_test = false;
		goto exit;
	}

	wait_for_completion(&thread_finish);

exit:
	mutex_unlock(&test_status_mutex);
}

int get_dcih_notify_test_result(void)
{
	int ret = -EINVAL;

	mutex_lock(&test_status_mutex);

	start_notify_test = false;
	if (test_notify_thread) {
		send_sig(SIGTERM, test_notify_thread, 0);
		ret = kthread_stop(test_notify_thread);
		test_notify_thread = NULL;
	}

	mutex_unlock(&test_status_mutex);

	return ret;
}

void start_dcih_wait_notify_test(uint32_t driver_id)
{
	uintptr_t data = driver_id;

	mutex_lock(&test_status_mutex);

	if (start_wait_notify_test) {
		IMSG_ERROR("test thread is still running\n");
		goto exit;
	}

	start_wait_notify_test = true;

	test_wait_notify_thread = kthread_run(wait_driver_notify_test,
				(void *)data, "dcih_wait_notify_test_thread");
	if (IS_ERR(test_wait_notify_thread)) {
		IMSG_ERROR("create dcih test wait notify thread failed\n");
		test_wait_notify_thread = NULL;
		start_wait_notify_test = false;
		goto exit;
	}

	wait_for_completion(&thread_start);

exit:
	mutex_unlock(&test_status_mutex);

}

int get_dcih_wait_notify_test_result(void)
{
	int ret = -EINVAL;

	mutex_lock(&test_status_mutex);

	start_wait_notify_test = false;
	if (test_wait_notify_thread) {
		send_sig(SIGTERM, test_wait_notify_thread, 0);
		ret = kthread_stop(test_wait_notify_thread);
		test_wait_notify_thread = NULL;
	}

	mutex_unlock(&test_status_mutex);

	return ret;
}
