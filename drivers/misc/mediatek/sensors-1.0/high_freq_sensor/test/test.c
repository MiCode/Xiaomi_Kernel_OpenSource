/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

#include "hf_manager.h"

struct test_device {
	struct hf_device hf_dev;
};

struct test_device test_driver1;
struct test_device test_driver2;
struct test_device test_driver3;
struct test_device test_driver4;

static unsigned char support_sensors1[] = {
	1,
};
static unsigned char support_sensors2[] = {
	2,
};
static unsigned char support_sensors3[] = {
	3,
};
static unsigned char support_sensors4[] = {
	4,
};

static int test_enable(struct hf_device *hfdev, int sensor_id, int en)
{
	pr_debug("%s id:%d en:%d\n", __func__, sensor_id, en);
	return 0;
}

static int test_batch(struct hf_device *hfdev, int sensor_id,
		int64_t delay, int64_t latency)
{
	pr_debug("%s id:%d delay:%lld latency:%lld\n", __func__, sensor_id,
		delay, latency);
	return 0;
}

static int test_sample(struct hf_device *hfdev)
{
	struct test_device *driver_dev = hf_device_get_private_data(hfdev);
	struct hf_manager *manager = driver_dev->hf_dev.manager;

	pr_debug("%s %s\n", __func__, driver_dev->hf_dev.dev_name);
	manager->complete(manager);
	return 0;
}

static int tests_init(void)
{
	int err = 0;

	test_driver1.hf_dev.dev_name = "test_driver1";
	test_driver1.hf_dev.device_poll = HF_DEVICE_IO_POLLING;
	test_driver1.hf_dev.device_bus = HF_DEVICE_IO_SYNC;
	test_driver1.hf_dev.support_list = support_sensors1;
	test_driver1.hf_dev.support_size = ARRAY_SIZE(support_sensors1);
	test_driver1.hf_dev.enable = test_enable;
	test_driver1.hf_dev.batch = test_batch;
	test_driver1.hf_dev.sample = test_sample;

	err = hf_manager_create(&test_driver1.hf_dev);
	if (err < 0)
		pr_err("%s hf_manager_create fail\n", __func__);
	hf_device_set_private_data(&test_driver1.hf_dev, &test_driver1);

	test_driver2.hf_dev.dev_name = "test_driver2";
	test_driver2.hf_dev.device_poll = HF_DEVICE_IO_POLLING;
	test_driver2.hf_dev.device_bus = HF_DEVICE_IO_SYNC;
	test_driver2.hf_dev.support_list = support_sensors2;
	test_driver2.hf_dev.support_size = ARRAY_SIZE(support_sensors2);
	test_driver2.hf_dev.enable = test_enable;
	test_driver2.hf_dev.batch = test_batch;
	test_driver2.hf_dev.sample = test_sample;

	err = hf_manager_create(&test_driver2.hf_dev);
	if (err < 0)
		pr_err("%s hf_manager_create fail\n", __func__);
	hf_device_set_private_data(&test_driver2.hf_dev, &test_driver2);

	test_driver3.hf_dev.dev_name = "test_driver3";
	test_driver3.hf_dev.device_poll = HF_DEVICE_IO_POLLING;
	test_driver3.hf_dev.device_bus = HF_DEVICE_IO_ASYNC;
	test_driver3.hf_dev.support_list = support_sensors3;
	test_driver3.hf_dev.support_size = ARRAY_SIZE(support_sensors3);
	test_driver3.hf_dev.enable = test_enable;
	test_driver3.hf_dev.batch = test_batch;
	test_driver3.hf_dev.sample = test_sample;

	err = hf_manager_create(&test_driver3.hf_dev);
	if (err < 0)
		pr_err("%s hf_manager_create fail\n", __func__);
	hf_device_set_private_data(&test_driver3.hf_dev, &test_driver3);

	test_driver4.hf_dev.dev_name = "test_driver4";
	test_driver4.hf_dev.device_poll = HF_DEVICE_IO_POLLING;
	test_driver4.hf_dev.device_bus = HF_DEVICE_IO_ASYNC;
	test_driver4.hf_dev.support_list = support_sensors4;
	test_driver4.hf_dev.support_size = ARRAY_SIZE(support_sensors4);
	test_driver4.hf_dev.enable = test_enable;
	test_driver4.hf_dev.batch = test_batch;
	test_driver4.hf_dev.sample = test_sample;

	err = hf_manager_create(&test_driver4.hf_dev);
	if (err < 0)
		pr_err("%s hf_manager_create fail\n", __func__);
	hf_device_set_private_data(&test_driver4.hf_dev, &test_driver4);
	return 0;
}

static int __init test_init(void)
{
	tests_init();
	return 0;
}

static void __exit test_exit(void)
{

}

module_init(test_init);
module_exit(test_exit);


MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("test driver");
MODULE_LICENSE("GPL");
