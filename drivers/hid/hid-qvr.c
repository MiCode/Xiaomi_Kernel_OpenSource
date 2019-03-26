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
#include <linux/timekeeping.h>
#include <linux/ion.h>
#include "../soc/qcom/smp2p_private.h"
#include "hid-ids.h"
#include "hid-qvr.h"
#include "hid-trace.h"

static struct ion_handle *handle;
static struct ion_client *client;
static void *vaddr;
static size_t vsize;
static uint64_t ts_base;
static uint64_t ts_offset;
static int msg_size = 368;

struct gpio_info {
	int gpio_base_id;
	int irq_base_id;
};


/* GPIO Inbound/Outbound callback info */
struct gpio_inout {
	struct gpio_info in;
	struct gpio_info out;
};

static struct gpio_inout gpio_info[SMP2P_NUM_PROCS];
static struct gpio_info *in_gpio_info_ptr;
static struct gpio_info *out_gpio_info_ptr;


static struct hid_driver qvr_external_sensor_driver;
static int fd;


struct ion_handle {
	struct kref ref;
	unsigned int user_ref_count;
	struct ion_client *client;
	struct ion_buffer *buffer;
	struct rb_node node;
	unsigned int kmap_cnt;
	int id;
};

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

	pr_debug("%s: gts= %llu, gx= %d, gy=%d, gz=%d", __func__,
		data->gts, data->gx, data->gy, data->gz);
	pr_debug("%s: ats= %llu, ax= %d, ay=%d, az=%d", __func__,
		data->ats, data->ax, data->ay, data->az);
	pr_debug("%s: mts= %llu, mx= %d, my=%d, mz=%d", __func__,
		data->mts, data->mx, data->my, data->mz);

	index_buf->most_recent_index = buf_index;
	buf_index = (buf_index == (8 - 1)) ? 0 : buf_index + 1;
	return 0;
}

static int register_smp2p(char *node_name, struct gpio_info *gpio_info_ptr)
{
	struct device_node *node = NULL;
	int cnt = 0;
	int id = 0;

	node = of_find_compatible_node(NULL, NULL, node_name);
	if (node) {
		cnt = of_gpio_count(node);
		if (cnt && gpio_info_ptr) {
			id = of_get_gpio(node, 0);
			if (id == -EPROBE_DEFER)
				return id;
			gpio_info_ptr->gpio_base_id = id;
			gpio_info_ptr->irq_base_id = gpio_to_irq(id);
			return 0;
		}
	}
	return -EINVAL;
}



static int kernel_map_gyro_buffer(int fd)
{
	handle = ion_import_dma_buf_fd(client, fd);
	if (IS_ERR(handle)) {
		pr_err("%s: ion_import_dma_buf_fd failed\n", __func__);
		return -EINVAL;
	}

	if (ion_handle_get_size(client, handle, &vsize)) {
		pr_err("%s: Could not dma buf %d size\n", __func__, fd);
		return -EINVAL;
	}

	vaddr = ion_map_kernel(client, handle);
	if (IS_ERR_OR_NULL(vaddr)) {
		ion_free(client, handle);
		return -EINVAL;
	}

	return 0;
}


static void kernel_unmap_gyro_buffer(void)
{
	if (!IS_ERR_OR_NULL(vaddr)) {
		ion_unmap_kernel(client, handle);
		ion_free(client, handle);
		vaddr = NULL;
	}
}

static ssize_t fd_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, 16, "%d\n", fd);
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
	char *in_node_name = "qcom,smp2pgpio_client_qvrexternal_5_in";
	char *out_node_name = "qcom,smp2pgpio_client_qvrexternal_5_out";
	__u8 hid_buf[255] = { 0 };
	size_t hid_count = 64;

	ret = register_smp2p(in_node_name, in_gpio_info_ptr);
	if (ret) {
		pr_err("%s: register_smp2p failed", __func__);
		goto err_free;
	}
	ret = register_smp2p(out_node_name, out_gpio_info_ptr);
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
	hid_buf[0] = 2;
	hid_buf[1] = 7;
	ret = hid_hw_raw_request(hdev, hid_buf[0],
		hid_buf,
		hid_count,
		HID_FEATURE_REPORT,
		HID_REQ_SET_REPORT);
	return 0;
err_free:
	return ret;

}

static int qvr_external_sensor_raw_event(struct hid_device *hid,
	struct hid_report *report,
	u8 *data, int size)
{
	int val;
	int ret = -1;

	if (vaddr != NULL && report->id == 0x1) {
		ret = qvr_send_package_wrap(data/*hid_value*/, size, hid);
		if (ret != 0) {
			pr_err("%s: qvr_send_package_wrap failed", __func__);
			return ret;
		}
		val = 1 ^ gpio_get_value(out_gpio_info_ptr->gpio_base_id + 0);
		gpio_set_value(out_gpio_info_ptr->gpio_base_id + 0, val);
		ret = -1;
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
	const char *device_name = "aoe";
	int ret = 0;

	in_gpio_info_ptr = &gpio_info[SMP2P_CDSP_PROC].in;
	in_gpio_info_ptr->gpio_base_id = -1;
	out_gpio_info_ptr = &gpio_info[SMP2P_CDSP_PROC].out;
	out_gpio_info_ptr->gpio_base_id = -1;


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

	client = msm_ion_client_create(device_name);
	if (client == NULL) {
		pr_err("msm_ion_client_create failed in %s", __func__);
		return -EINVAL;
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

