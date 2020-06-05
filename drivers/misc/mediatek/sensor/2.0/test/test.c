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

static struct sensor_info support_sensors1[] = {
	{
		.sensor_type = SENSOR_TYPE_ACCELEROMETER,
		.gain = 1,
		.name = {'a', 'c', 'c', 'e', 'l'},
		.vendor = {'m', 't', 'k'},
	},
};
static struct sensor_info support_sensors2[] = {
	{
		.sensor_type = SENSOR_TYPE_MAGNETIC_FIELD,
		.gain = 1,
		.name = {'m', 'a', 'g'},
		.vendor = {'m', 't', 'k'},
	},
};
static struct sensor_info support_sensors3[] = {
	{
		.sensor_type = SENSOR_TYPE_GYROSCOPE,
		.gain = 1,
		.name = {'g', 'y', 'r', 'o'},
		.vendor = {'m', 't', 'k'},
	},
};
static struct sensor_info support_sensors4[] = {
	{
		.sensor_type = SENSOR_TYPE_PRESSURE,
		.gain = 1,
		.name = {'p', 'r', 'e', 's', 's'},
		.vendor = {'m', 't', 'k'},
	},
};

static int test_enable(struct hf_device *hfdev, int sensor_type, int en)
{
	pr_debug("%s id:%d en:%d\n", __func__, sensor_type, en);
	return 0;
}

static int test_batch(struct hf_device *hfdev, int sensor_type,
		int64_t delay, int64_t latency)
{
	pr_debug("%s id:%d delay:%lld latency:%lld\n", __func__, sensor_type,
		delay, latency);
	return 0;
}

static int test_sample(struct hf_device *hfdev)
{
	struct test_device *driver_dev = hf_device_get_private_data(hfdev);
	struct hf_manager *manager = driver_dev->hf_dev.manager;
	struct hf_manager_event event;

	pr_debug("%s %s\n", __func__, driver_dev->hf_dev.dev_name);

	memset(&event, 0, sizeof(struct hf_manager_event));
	event.timestamp = get_interrupt_timestamp(manager);
	event.sensor_type = driver_dev->hf_dev.support_list[0].sensor_type;
	event.accurancy = SENSOR_ACCURANCY_HIGH;
	event.action = DATA_ACTION;
	event.word[0] = 0;
	event.word[1] = 0;
	event.word[2] = 0;
	manager->report(manager, &event);
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
	if (err < 0) {
		pr_err("%s hf_manager_create fail\n", __func__);
		goto out1;
	}
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
	if (err < 0) {
		pr_err("%s hf_manager_create fail\n", __func__);
		goto out2;
	}
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
	if (err < 0) {
		pr_err("%s hf_manager_create fail\n", __func__);
		goto out3;
	}
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
	if (err < 0) {
		pr_err("%s hf_manager_create fail\n", __func__);
		goto out4;
	}
	hf_device_set_private_data(&test_driver4.hf_dev, &test_driver4);
	return 0;

out4:
	hf_manager_destroy(test_driver3.hf_dev.manager);
out3:
	hf_manager_destroy(test_driver2.hf_dev.manager);
out2:
	hf_manager_destroy(test_driver1.hf_dev.manager);
out1:
	return -EINVAL;
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

MODULE_DESCRIPTION("high frequency manager test");
MODULE_AUTHOR("Hongxu Zhao <hongxu.zhao@mediatek.com>");
MODULE_LICENSE("GPL v2");
