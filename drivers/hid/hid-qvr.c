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
 *
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/dma-buf.h>
#include <linux/msm_ion.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/hiddev.h>
#include <linux/hid-debug.h>
#include <linux/hidraw.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/timekeeping.h>
#include <linux/soc/qcom/smem_state.h>
#include "hid-ids.h"
#include "hid-qvr.h"
#include "hid-trace.h"

static struct dma_buf *qvr_buf;
static void *vaddr;
static size_t vsize;
static uint64_t ts_base;
static uint64_t ts_offset;

struct gpio_info {
	unsigned int smem_bit;
	struct qcom_smem_state *smem_state;
};


static struct device *qvr_device;
static struct gpio_info gpio_info_out;

static struct hid_driver qvr_external_sensor_driver;
static int fd;

const static int msg_size = 368;
const static int hid_request_report_id = 2;
const static int hid_request_report_size = 64;

struct qvr_buf_index {
	int most_recent_index;
	uint8_t padding[60];
};

struct qvr_sensor_t {
	uint64_t gts;
	uint64_t ats;
	uint64_t mts;
	s32 gx;
	s32 gy;
	s32 gz;
	s32 ax;
	s32 ay;
	s32 az;
	s32 mx;
	s32 my;
	s32 mz;
	uint8_t padding[4];
};


int qvr_send_package_wrap(u8 *message, int msize, struct hid_device *hid)
{
	struct qvr_sensor_t *sensor_buf;
	struct qvr_sensor_t *data;
	static int buf_index;
	struct external_imu_format imuData = { 0 };
	struct qvr_buf_index *index_buf;

	/*
	 * Actual message size is 369 bytes
	 * to make it 8 byte aligned we created a structure of size 368 bytes.
	 * Ignoring the first byte 'report id' (which is always 1)
	 *
	 */
	memcpy((void *)&imuData, (void *)message + 1, msg_size);

	if (!ts_base)
		ts_base = ktime_to_ns(ktime_get_boottime());
	if (!ts_offset)
		ts_offset = imuData.gts0;
	index_buf = (struct qvr_buf_index *)
		((uintptr_t)vaddr + (vsize / 2) + (8 * sizeof(*sensor_buf)));
	sensor_buf = (struct qvr_sensor_t *)((uintptr_t)vaddr + (vsize / 2));

	data = (struct qvr_sensor_t *)&(sensor_buf[buf_index]);
	if (ts_offset > imuData.gts0)
		data->ats = ts_base + ((ts_offset - imuData.gts0) * 100);
	else
		data->ats = ts_base + ((imuData.gts0 - ts_offset) * 100);
	if (imuData.mts0 == 0)
		data->mts = 0;
	else
		data->mts = data->ats;
	data->gts = data->ats;
	data->ax = -imuData.ax0;
	data->ay = imuData.ay0;
	data->az = -imuData.az0;
	data->gx = -imuData.gx0;
	data->gy = imuData.gy0;
	data->gz = -imuData.gz0;
	data->mx = -imuData.my0;
	data->my = -imuData.mx0;
	data->mz = -imuData.mz0;

	trace_qvr_recv_sensor("gyro", data->gts, data->gx, data->gy, data->gz);
	trace_qvr_recv_sensor("accel", data->ats, data->ax, data->ay, data->az);

	index_buf->most_recent_index = buf_index;
	buf_index = (buf_index == (8 - 1)) ? 0 : buf_index + 1;
	return 0;
}

static int register_smp2p(struct device *dev, char *node_name,
	struct gpio_info *gpio_info_ptr)
{
	struct device_node *node = dev->of_node;

	if (!gpio_info_ptr)
		return -EINVAL;
	if (node == NULL) {
		pr_debug("%s: device node NULL\n", __func__);
		dev->of_node = of_find_compatible_node(NULL, NULL, node_name);
		node = dev->of_node;
	}
	if (!of_find_property(node, "qcom,smem-states", NULL))
		return -EINVAL;
	gpio_info_ptr->smem_state = qcom_smem_state_get(dev,
		"qvrexternal-smp2p-out",
		&gpio_info_ptr->smem_bit);
	pr_debug("%s: state: %pK, bit: %d\n", __func__,
		gpio_info_ptr->smem_state,
		gpio_info_ptr->smem_bit);
	if (IS_ERR_OR_NULL(gpio_info_ptr->smem_state)) {
		pr_debug("%s: Error smem_state\n", __func__);
		return PTR_ERR(gpio_info_ptr->smem_state);
	}

	return 0;

}
static int kernel_map_gyro_buffer(int fd)
{
	int ret = 0;

	qvr_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(qvr_buf)) {
		ret = -ENOMEM;
		pr_err("dma_buf_get failed for fd: %d\n", fd);
		goto done;
	}
	ret = dma_buf_begin_cpu_access(qvr_buf, DMA_BIDIRECTIONAL);
	if (ret) {
		pr_err("%s: dma_buf_begin_cpu_access failed\n", __func__);
		goto err_dma;
	}
	vsize = qvr_buf->size;
	vaddr = dma_buf_kmap(qvr_buf, 0);
	if (IS_ERR_OR_NULL(vaddr)) {
		ret = -ENOMEM;
		pr_err("dma_buf_kmap failed for fd: %d\n", fd);
		goto err_end_access;
	}

	return 0;

err_end_access:
	dma_buf_end_cpu_access(qvr_buf, DMA_BIDIRECTIONAL);
err_dma:
	dma_buf_put(qvr_buf);
	qvr_buf = NULL;
done:
	return ret;

}


static void kernel_unmap_gyro_buffer(void)
{
	if (IS_ERR_OR_NULL(vaddr))
		return;
	dma_buf_kunmap(qvr_buf, 0, vaddr);
	dma_buf_end_cpu_access(qvr_buf, DMA_BIDIRECTIONAL);
	vaddr = NULL;
	dma_buf_put(qvr_buf);
	qvr_buf = NULL;
}

static ssize_t fd_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, sizeof(buf), "%d\n", fd);
}

static ssize_t fd_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 10, &fd);
	if (ret < 0)
		return ret;
	if (fd == -1)
		kernel_unmap_gyro_buffer();
	else
		kernel_map_gyro_buffer(fd);
	ts_base = 0;
	ts_offset = 0;

	return count;
}

static ssize_t ts_base_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return  snprintf(buf, 16, "%lld\n", ts_base);
}

static ssize_t ts_base_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	return 0;
}

static ssize_t ts_offset_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return  snprintf(buf, 16, "%lld\n", ts_offset * 100);
}

static ssize_t ts_offset_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	return 0;
}

static struct kobj_attribute fd_attribute = __ATTR(fd, 0664,
	fd_show,
	fd_store);
static struct kobj_attribute ts_base_attribute = __ATTR(ts_base, 0664,
	ts_base_show,
	ts_base_store);
static struct kobj_attribute ts_offset_attribute = __ATTR(ts_offset, 0664,
	ts_offset_show,
	ts_offset_store);

static struct attribute *attrs[] = {
	&fd_attribute.attr,
	&ts_base_attribute.attr,
	&ts_offset_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *qvr_external_sensor_kobj;

static int qvr_external_sensor_probe(struct hid_device *hdev,
	const struct hid_device_id *id)
{
	int ret;
	char *node_name = "qcom,smp2p-interrupt-qvrexternal-5-out";
	__u8 *hid_buf;

	ret = register_smp2p(&hdev->dev, node_name, &gpio_info_out);
	if (ret) {
		pr_err("%s: register_smp2p failed", __func__);
		goto err_free;
	}
	ret = hid_open_report(hdev);
	if (ret) {
		pr_err("%s: hid_open_report failed", __func__);
		goto err_free;
	}
	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		pr_err("%s: hid_hw_start failed", __func__);
		goto err_free;
	}
	hid_buf = kzalloc(255, GFP_ATOMIC);
	if (hid_buf == NULL)
		return -ENOMEM;
	hid_buf[0] = hid_request_report_id;
	hid_buf[1] = 7;
	ret = hid_hw_raw_request(hdev, hid_buf[0], hid_buf,
		hid_request_report_size,
		HID_FEATURE_REPORT,
		HID_REQ_SET_REPORT);
	kfree(hid_buf);

	qvr_device = &hdev->dev;

	return 0;

err_free:
	return ret;

}

static int qvr_external_sensor_raw_event(struct hid_device *hid,
	struct hid_report *report,
	u8 *data, int size)
{
	static int val;
	int ret = -1;

	if (vaddr != NULL && report->id == 0x1) {
		ret = qvr_send_package_wrap(data/*hid_value*/, size, hid);
		if (ret == 0) {
			val = 1 ^ val;
			qcom_smem_state_update_bits(gpio_info_out.smem_state,
				BIT(gpio_info_out.smem_bit), val);
			ret = -1;
		}
	}
	return ret;
}

static void qvr_external_sensor_device_remove(struct hid_device *hdev)
{
	hid_hw_stop(hdev);
}

static struct hid_device_id qvr_external_sensor_table[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_QVR5, USB_DEVICE_ID_QVR5) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_QVR32A, USB_DEVICE_ID_QVR32A) },
	{ }
};
MODULE_DEVICE_TABLE(hid, qvr_external_sensor_table);

static struct hid_driver qvr_external_sensor_driver = {
	.name = "qvr_external_sensor",
	.id_table = qvr_external_sensor_table,
	.probe = qvr_external_sensor_probe,
	.raw_event = qvr_external_sensor_raw_event,
	.remove = qvr_external_sensor_device_remove,
};

module_hid_driver(qvr_external_sensor_driver);

static int __init qvr_external_sensor_init(void)
{
	int ret = 0;

	qvr_external_sensor_kobj =
		kobject_create_and_add("qvr_external_sensor", kernel_kobj);
	if (!qvr_external_sensor_kobj) {
		pr_err("%s: kobject_create_and_add() fail\n", __func__);
		return -ENOMEM;
	}
	ret = sysfs_create_group(qvr_external_sensor_kobj, &attr_group);
	if (ret) {
		pr_err("%s: can't register sysfs\n", __func__);
		return -ENOMEM;
	}

	return ret;
}

static void __exit qvr_external_sensor_exit(void)
{
	kobject_put(qvr_external_sensor_kobj);
}

module_init(qvr_external_sensor_init);
module_exit(qvr_external_sensor_exit);
MODULE_LICENSE("GPL v2");

