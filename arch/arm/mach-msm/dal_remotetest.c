/* Copyright (c) 2008-2009, The Linux Foundation. All rights reserved.
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
/*
 * DAL remote test device test suite.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/debugfs.h>

#include "dal_remotetest.h"

#define BYTEBUF_LEN 64

#define rpc_error(num)							\
	do {								\
		errmask |= (1 << num);					\
		printk(KERN_INFO "%s: remote_unittest_%d failed (%d)\n", \
		       __func__, num, ret);				\
	} while (0)

#define verify_error(num, field)					\
	do {								\
		errmask |= (1 << num);					\
		printk(KERN_INFO "%s: remote_unittest_%d failed (%s)\n", \
		       __func__, num, field);				\
	} while (0)


static struct dentry *debugfs_dir_entry;
static struct dentry *debugfs_modem_entry;
static struct dentry *debugfs_dsp_entry;

static uint8_t in_bytebuf[BYTEBUF_LEN];
static uint8_t out_bytebuf[BYTEBUF_LEN];
static uint8_t out_bytebuf2[BYTEBUF_LEN];
static struct remote_test_data in_data;
static struct remote_test_data out_data;
static int block_until_cb = 1;

static void init_data(struct remote_test_data *data)
{
	int i;
	data->regular_event = REMOTE_UNITTEST_INPUT_HANDLE;
	data->payload_event = REMOTE_UNITTEST_INPUT_HANDLE;
	for (i = 0; i < 32; i++)
		data->test[i] = i;
}

static int verify_data(struct remote_test_data *data)
{
	int i;
	if (data->regular_event != REMOTE_UNITTEST_INPUT_HANDLE ||
	    data->payload_event != REMOTE_UNITTEST_INPUT_HANDLE)
		return -1;
	for (i = 0; i < 32; i++)
		if (data->test[i] != i)
			return -1;

	return 0;
}

static int verify_uint32_buffer(uint32_t *buf)
{
	int i;
	for (i = 0; i < 32; i++)
		if (buf[i] != i)
			return -1;

	return 0;
}

static void init_bytebuf(uint8_t *bytebuf)
{
	int i;
	for (i = 0; i < BYTEBUF_LEN; i++)
		bytebuf[i] = i & 0xff;
}

static int verify_bytebuf(uint8_t *bytebuf)
{
	int i;
	for (i = 0; i < BYTEBUF_LEN; i++)
		if (bytebuf[i] != (i & 0xff))
			return -1;

	return 0;
}

static void test_cb(void *context, uint32_t param, void *data, uint32_t len)
{
	block_until_cb = 0;
}

static int remotetest_exec(int dest, u64 *val)
{
	void *dev_handle;
	void *event_handles[3];
	void *cb_handle;
	int ret;
	u64 errmask = 0;
	uint32_t ouint;
	uint32_t oalen;

	/* test daldevice_attach */
	ret = daldevice_attach(REMOTE_UNITTEST_DEVICEID, NULL,
			       dest, &dev_handle);
	if (ret) {
		printk(KERN_INFO "%s: failed to attach (%d)\n", __func__, ret);
		*val = 0xffffffff;
		return 0;
	}

	/* test remote_unittest_0 */
	ret = remote_unittest_0(dev_handle, REMOTE_UNITTEST_INARG_1);
	if (ret)
		rpc_error(0);

	/* test remote_unittest_1 */
	ret = remote_unittest_1(dev_handle, REMOTE_UNITTEST_INARG_1,
				REMOTE_UNITTEST_INARG_2);
	if (ret)
		rpc_error(1);

	/* test remote_unittest_2 */
	ouint = 0;
	ret = remote_unittest_2(dev_handle, REMOTE_UNITTEST_INARG_1, &ouint);
	if (ret)
		rpc_error(2);
	else if (ouint != REMOTE_UNITTEST_OUTARG_1)
		verify_error(2, "ouint");

	/* test remote_unittest_3 */
	ret = remote_unittest_3(dev_handle, REMOTE_UNITTEST_INARG_1,
				REMOTE_UNITTEST_INARG_2,
				REMOTE_UNITTEST_INARG_3);
	if (ret)
		rpc_error(3);

	/* test remote_unittest_4 */
	ouint = 0;
	ret = remote_unittest_4(dev_handle, REMOTE_UNITTEST_INARG_1,
				REMOTE_UNITTEST_INARG_2, &ouint);
	if (ret)
		rpc_error(4);
	else if (ouint != REMOTE_UNITTEST_OUTARG_1)
		verify_error(4, "ouint");

	/* test remote_unittest_5 */
	init_data(&in_data);
	ret = remote_unittest_5(dev_handle, &in_data, sizeof(in_data));
	if (ret)
		rpc_error(5);

	/* test remote_unittest_6 */
	init_data(&in_data);
	ret = remote_unittest_6(dev_handle, REMOTE_UNITTEST_INARG_1,
				&in_data.test, sizeof(in_data.test));
	if (ret)
		rpc_error(6);

	/* test remote_unittest_7 */
	init_data(&in_data);
	memset(&out_data, 0, sizeof(out_data));
	ret = remote_unittest_7(dev_handle, &in_data, sizeof(in_data),
				&out_data.test, sizeof(out_data.test),
				&oalen);
	if (ret)
		rpc_error(7);
	else if (oalen != sizeof(out_data.test))
		verify_error(7, "oalen");
	else if (verify_uint32_buffer(out_data.test))
		verify_error(7, "obuf");

	/* test remote_unittest_8 */
	init_bytebuf(in_bytebuf);
	memset(&out_data, 0, sizeof(out_data));
	ret = remote_unittest_8(dev_handle, in_bytebuf, sizeof(in_bytebuf),
				&out_data, sizeof(out_data));
	if (ret)
		rpc_error(8);
	else if (verify_data(&out_data))
		verify_error(8, "obuf");

	/* test remote_unittest_9 */
	memset(&out_bytebuf, 0, sizeof(out_bytebuf));
	ret = remote_unittest_9(dev_handle, out_bytebuf, sizeof(out_bytebuf));
	if (ret)
		rpc_error(9);
	else if (verify_bytebuf(out_bytebuf))
		verify_error(9, "obuf");

	/* test remote_unittest_10 */
	init_bytebuf(in_bytebuf);
	memset(&out_bytebuf, 0, sizeof(out_bytebuf));
	ret = remote_unittest_10(dev_handle, REMOTE_UNITTEST_INARG_1,
				 in_bytebuf, sizeof(in_bytebuf),
				 out_bytebuf, sizeof(out_bytebuf), &oalen);
	if (ret)
		rpc_error(10);
	else if (oalen != sizeof(out_bytebuf))
		verify_error(10, "oalen");
	else if (verify_bytebuf(out_bytebuf))
		verify_error(10, "obuf");

	/* test remote_unittest_11 */
	memset(&out_bytebuf, 0, sizeof(out_bytebuf));
	ret = remote_unittest_11(dev_handle, REMOTE_UNITTEST_INARG_1,
				 out_bytebuf, sizeof(out_bytebuf));
	if (ret)
		rpc_error(11);
	else if (verify_bytebuf(out_bytebuf))
		verify_error(11, "obuf");

	/* test remote_unittest_12 */
	memset(&out_bytebuf, 0, sizeof(out_bytebuf));
	ret = remote_unittest_12(dev_handle, REMOTE_UNITTEST_INARG_1,
				 out_bytebuf, sizeof(out_bytebuf), &oalen);
	if (ret)
		rpc_error(12);
	else if (oalen != sizeof(out_bytebuf))
		verify_error(12, "oalen");
	else if (verify_bytebuf(out_bytebuf))
		verify_error(12, "obuf");

	/* test remote_unittest_13 */
	init_data(&in_data);
	memset(&out_data, 0, sizeof(out_data));
	ret = remote_unittest_13(dev_handle, in_data.test, sizeof(in_data.test),
				 &in_data, sizeof(in_data),
				 &out_data, sizeof(out_data));
	if (ret)
		rpc_error(13);
	else if (verify_data(&out_data))
		verify_error(13, "obuf");

	/* test remote_unittest_14 */
	init_data(&in_data);
	memset(out_bytebuf, 0, sizeof(out_bytebuf));
	memset(out_bytebuf2, 0, sizeof(out_bytebuf2));
	ret = remote_unittest_14(dev_handle,
				 in_data.test, sizeof(in_data.test),
				 out_bytebuf, sizeof(out_bytebuf),
				 out_bytebuf2, sizeof(out_bytebuf2), &oalen);
	if (ret)
		rpc_error(14);
	else if (verify_bytebuf(out_bytebuf))
		verify_error(14, "obuf");
	else if (oalen != sizeof(out_bytebuf2))
		verify_error(14, "oalen");
	else if (verify_bytebuf(out_bytebuf2))
		verify_error(14, "obuf2");

	/* test remote_unittest_15 */
	init_data(&in_data);
	memset(out_bytebuf, 0, sizeof(out_bytebuf));
	memset(&out_data, 0, sizeof(out_data));
	ret = remote_unittest_15(dev_handle,
				 in_data.test, sizeof(in_data.test),
				 &in_data, sizeof(in_data),
				 &out_data, sizeof(out_data), &oalen,
				 out_bytebuf, sizeof(out_bytebuf));
	if (ret)
		rpc_error(15);
	else if (oalen != sizeof(out_data))
		verify_error(15, "oalen");
	else if (verify_bytebuf(out_bytebuf))
		verify_error(15, "obuf");
	else if (verify_data(&out_data))
		verify_error(15, "obuf2");

	/* test setting up asynch events */
	event_handles[0] = dalrpc_alloc_event(dev_handle);
	event_handles[1] = dalrpc_alloc_event(dev_handle);
	event_handles[2] = dalrpc_alloc_event(dev_handle);
	cb_handle = dalrpc_alloc_cb(dev_handle, test_cb, &out_data);
	in_data.regular_event = (uint32_t)event_handles[2];
	in_data.payload_event = (uint32_t)cb_handle;
	ret = remote_unittest_eventcfg(dev_handle, &in_data, sizeof(in_data));
	if (ret) {
		errmask |= (1 << 16);
		printk(KERN_INFO "%s: failed to configure asynch (%d)\n",
		       __func__, ret);
	}

	/* test event */
	ret = remote_unittest_eventtrig(dev_handle,
					REMOTE_UNITTEST_REGULAR_EVENT);
	if (ret) {
		errmask |= (1 << 17);
		printk(KERN_INFO "%s: failed to trigger event (%d)\n",
		       __func__, ret);
	}
	ret = dalrpc_event_wait(event_handles[2], 1000);
	if (ret) {
		errmask |= (1 << 18);
		printk(KERN_INFO "%s: failed to receive event (%d)\n",
		       __func__, ret);
	}

	/* test event again */
	ret = remote_unittest_eventtrig(dev_handle,
					REMOTE_UNITTEST_REGULAR_EVENT);
	if (ret) {
		errmask |= (1 << 19);
		printk(KERN_INFO "%s: failed to trigger event (%d)\n",
		       __func__, ret);
	}
	ret = dalrpc_event_wait_multiple(3, event_handles, 1000);
	if (ret != 2) {
		errmask |= (1 << 20);
		printk(KERN_INFO "%s: failed to receive event (%d)\n",
		       __func__, ret);
	}

	/* test callback */
	ret = remote_unittest_eventtrig(dev_handle,
					REMOTE_UNITTEST_CALLBACK_EVENT);
	if (ret) {
		errmask |= (1 << 21);
		printk(KERN_INFO "%s: failed to trigger callback (%d)\n",
		       __func__, ret);
	} else
		while (block_until_cb)
			;

	dalrpc_dealloc_cb(dev_handle, cb_handle);
	dalrpc_dealloc_event(dev_handle, event_handles[0]);
	dalrpc_dealloc_event(dev_handle, event_handles[1]);
	dalrpc_dealloc_event(dev_handle, event_handles[2]);

	/* test daldevice_detach */
	ret = daldevice_detach(dev_handle);
	if (ret) {
		errmask |= (1 << 22);
		printk(KERN_INFO "%s: failed to detach (%d)\n", __func__, ret);
	}

	printk(KERN_INFO "%s: remote_unittest complete\n", __func__);

	*val = errmask;
	return 0;
}

static int remotetest_modem_exec(void *data, u64 *val)
{
	return remotetest_exec(DALRPC_DEST_MODEM, val);
}

static int remotetest_dsp_exec(void *data, u64 *val)
{
	return remotetest_exec(DALRPC_DEST_QDSP, val);
}

DEFINE_SIMPLE_ATTRIBUTE(dal_modemtest_fops, remotetest_modem_exec,
			NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(dal_dsptest_fops, remotetest_dsp_exec,
			NULL, "%llu\n");

static int __init remotetest_init(void)
{
	debugfs_dir_entry = debugfs_create_dir("dal", 0);
	if (IS_ERR(debugfs_dir_entry))
		return PTR_ERR(debugfs_dir_entry);

	debugfs_modem_entry = debugfs_create_file("modem_test", 0444,
						  debugfs_dir_entry,
						  NULL, &dal_modemtest_fops);
	if (IS_ERR(debugfs_modem_entry)) {
		debugfs_remove(debugfs_dir_entry);
		return PTR_ERR(debugfs_modem_entry);
	}

	debugfs_dsp_entry = debugfs_create_file("dsp_test", 0444,
					    debugfs_dir_entry,
					    NULL, &dal_dsptest_fops);
	if (IS_ERR(debugfs_dsp_entry)) {
		debugfs_remove(debugfs_modem_entry);
		debugfs_remove(debugfs_dir_entry);
		return PTR_ERR(debugfs_dsp_entry);
	}

	return 0;
}

static void __exit remotetest_exit(void)
{
	debugfs_remove(debugfs_modem_entry);
	debugfs_remove(debugfs_dsp_entry);
	debugfs_remove(debugfs_dir_entry);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Test for DAL RPC");
MODULE_VERSION("1.0");

module_init(remotetest_init);
module_exit(remotetest_exit);
