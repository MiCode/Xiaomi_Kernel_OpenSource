// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include "hid-ids.h"
#include "hid-qvr.h"
#include "hid-trace.h"

#define TIME_OUT_START_STOP_MS 500
#define TIME_OUT_READ_WRITE_MS 20

#define QVR_START_IMU		_IO('q', 1)
#define QVR_STOP_IMU		_IO('q', 2)
#define QVR_READ_CALIB_DATA_LEN	_IOR('q', 3, int32_t)
#define QVR_READ_CALIB_DATA	_IOR('q', 4, struct qvr_calib_data)

struct gpio_info {
	unsigned int smem_bit;
	struct qcom_smem_state *smem_state;
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

struct qvr_calib_data {
	__u64 data_ptr;
};

struct qvr_external_sensor {
	struct hid_device *hdev;
	struct device *device;
	struct dma_buf *qvr_buf;
	struct class *class;
	struct device *dev;
	void *vaddr;
	u8 *calib_data_pkt;
	struct cdev cdev;
	struct gpio_info gpio_info_out;
	dev_t dev_no;
	uint64_t ts_base;
	uint64_t ts_offset;
	size_t vsize;
	int calib_data_len;
	int calib_data_recv;
	int ext_ack;
	int fd;
};

static DECLARE_WAIT_QUEUE_HEAD(wq);
static struct qvr_external_sensor qvr_external_sensor;
static uint8_t DEBUG_ORIENTATION;

static int read_calibration_len(void)
{
	struct qvr_external_sensor *sensor = &qvr_external_sensor;
	__u8 *hid_buf;
	int ret;

	hid_buf = kzalloc(256, GFP_KERNEL);
	if (hid_buf == NULL)
		return -ENOMEM;

	hid_buf[0] = QVR_HID_REPORT_ID_CAL;
	hid_buf[1] = QVR_CMD_ID_CALIBRATION_DATA_SIZE;

	ret = hid_hw_raw_request(sensor->hdev, hid_buf[0],
		hid_buf,
		QVR_HID_REQUEST_REPORT_SIZE,
		HID_FEATURE_REPORT,
		HID_REQ_SET_REPORT);

	ret = wait_event_interruptible_timeout(wq,
		sensor->calib_data_len != -1,
		msecs_to_jiffies(TIME_OUT_READ_WRITE_MS));
	if (ret == 0) {
		kfree(hid_buf);
		return -ETIME;
	}

	kfree(hid_buf);
	return sensor->calib_data_len;
}

static uint8_t *read_calibration_data(void)
{
	struct qvr_external_sensor *sensor = &qvr_external_sensor;
	__u8 *hid_buf;
	int ret, total_read_len;
	uint8_t read_len;
	uint8_t *complete_data = NULL;

	if (sensor->calib_data_len < 0) {
		pr_err("%s: calibration data len missing\n", __func__);
		return NULL;
	}

	hid_buf = kzalloc(256, GFP_KERNEL);
	if (hid_buf == NULL)
		return NULL;

	hid_buf[0] = QVR_HID_REPORT_ID_CAL;
	hid_buf[1] = QVR_CMD_ID_CALIBRATION_BLOCK_DATA;

	complete_data = kzalloc(sensor->calib_data_len, GFP_KERNEL);
	if (complete_data == NULL) {
		kfree(hid_buf);
		return NULL;
	}
	total_read_len = 0;
	while (total_read_len < sensor->calib_data_len) {
		sensor->calib_data_recv = 0;
		ret = hid_hw_raw_request(sensor->hdev, hid_buf[0],
			hid_buf,
			QVR_HID_REQUEST_REPORT_SIZE,
			HID_FEATURE_REPORT,
			HID_REQ_SET_REPORT);
		ret = wait_event_interruptible_timeout(wq,
			sensor->calib_data_recv == 1,
			msecs_to_jiffies(TIME_OUT_READ_WRITE_MS));
		if (ret == 0) {
			pr_err("%s:get calibration data timeout\n", __func__);
			kfree(hid_buf);
			kfree(complete_data);
			return NULL;
		}
		if (sensor->calib_data_pkt == NULL) {
			kfree(hid_buf);
			kfree(complete_data);
			return NULL;
		}
		read_len = sensor->calib_data_pkt[2];
		if (total_read_len > sensor->calib_data_len - read_len) {
			kfree(hid_buf);
			kfree(complete_data);
			return NULL;
		}
		memcpy(&complete_data[total_read_len],
			&sensor->calib_data_pkt[3], read_len);
		total_read_len += read_len;
	}

	kfree(hid_buf);
	return complete_data;
}

static int control_imu_stream(bool status)
{
	struct qvr_external_sensor *sensor = &qvr_external_sensor;
	__u8 *hid_buf;
	int ret;

	sensor->ext_ack = 0;
	hid_buf = kzalloc(256, GFP_KERNEL);
	if (hid_buf == NULL)
		return -ENOMEM;

	hid_buf[0] = QVR_HID_REPORT_ID_CAL;
	hid_buf[1] = QVR_CMD_ID_IMU_CONTROL;
	hid_buf[2] = status;

	ret = hid_hw_raw_request(sensor->hdev, hid_buf[0],
		hid_buf,
		QVR_HID_REQUEST_REPORT_SIZE,
		HID_FEATURE_REPORT,
		HID_REQ_SET_REPORT);
	ret = wait_event_interruptible_timeout(wq, sensor->ext_ack == 1,
		msecs_to_jiffies(TIME_OUT_START_STOP_MS));
	if (!ret && status) {
		pr_debug("qvr: falling back - start IMU stream failed\n");
		hid_buf[0] = QVR_HID_REPORT_ID_CAL;
		hid_buf[1] = QVR_CMD_ID_IMU_CONTROL_FALLBACK;
		ret = hid_hw_raw_request(sensor->hdev, hid_buf[0], hid_buf,
			QVR_HID_REQUEST_REPORT_SIZE,
			HID_FEATURE_REPORT,
			HID_REQ_SET_REPORT);
	}
	kfree(hid_buf);
	if (ret > 0)
		return 0;

	return -ETIME;
}

static int qvr_send_package_wrap(u8 *message, int msize, struct hid_device *hid)
{
	struct qvr_external_sensor *sensor = &qvr_external_sensor;
	struct qvr_sensor_t *sensor_buf;
	struct qvr_sensor_t *data;
	static int buf_index;
	struct external_imu_format imuData = { 0 };
	struct qvr_buf_index *index_buf;

	if (msize != sizeof(struct external_imu_format)) {
		pr_err("%s: data size mismatch %d\n", __func__, msize);
		return -EPROTO;
	}

	memcpy((void *)&imuData, (void *)message,
		sizeof(struct external_imu_format));
	if (!sensor->ts_base) {
		if (imuData.gNumerator == 1 && imuData.aNumerator == 1)
			DEBUG_ORIENTATION = 1;
		else
			DEBUG_ORIENTATION = 0;
		pr_debug("qvr msize = %d reportID=%d padding=%d\n"
			"qvr version=%d numImu=%d nspip=%d pSize=%d\n"
			"qvr imuID=%d sampleID=%d temp=%d\n",
			msize, imuData.reportID, imuData.padding,
			imuData.version, imuData.numIMUs,
			imuData.numSamplesPerImuPacket,
			imuData.totalPayloadSize, imuData.imuID,
			imuData.sampleID, imuData.temperature);
		pr_debug("qvr gts0=%llu num=%d denom=%d\n"
			"qvr gx0=%d gy0=%d gz0=%d\n",
			imuData.gts0, imuData.gNumerator, imuData.gDenominator,
			imuData.gx0, imuData.gy0, imuData.gz0);
		pr_debug("qvr ats0=%llu num=%d denom=%d\n"
			"qvr ax0=%d ay0=%d az0=%d\n",
			imuData.ats0, imuData.aNumerator, imuData.aDenominator,
			imuData.ax0, imuData.ay0, imuData.az0);
		pr_debug("qvr mts0=%llu num=%d denom=%d\n"
			"mx0=%d my0=%d mz0=%d\n",
			imuData.mts0, imuData.mNumerator, imuData.mDenominator,
			imuData.mx0, imuData.my0, imuData.mz0);
	}
	if (!sensor->ts_base)
		sensor->ts_base = ktime_to_ns(ktime_get_boottime());
	if (!sensor->ts_offset)
		sensor->ts_offset = imuData.gts0;
	index_buf = (struct qvr_buf_index *)((uintptr_t)sensor->vaddr +
		(sensor->vsize / 2) + (8 * sizeof(*sensor_buf)));
	sensor_buf = (struct qvr_sensor_t *)((uintptr_t)sensor->vaddr +
		(sensor->vsize / 2));

	data = (struct qvr_sensor_t *)&(sensor_buf[buf_index]);
	if (sensor->ts_offset > imuData.gts0)
		data->ats = sensor->ts_base +
			sensor->ts_offset - imuData.gts0;
	else
		data->ats = sensor->ts_base +
			imuData.gts0 - sensor->ts_offset;
	if (imuData.mts0 == 0)
		data->mts = 0;
	else
		data->mts = data->ats;
	data->gts = data->ats;

	if (DEBUG_ORIENTATION == 1) {
		data->ax = -imuData.ax0;
		data->ay = imuData.ay0;
		data->az = -imuData.az0;
		data->gx = -imuData.gx0;
		data->gy = imuData.gy0;
		data->gz = -imuData.gz0;
		data->mx = -imuData.my0;
		data->my = -imuData.mx0;
		data->mz = -imuData.mz0;
	} else {
		data->ax = -imuData.ay0;
		data->ay = -imuData.ax0;
		data->az = -imuData.az0;
		data->gx = -imuData.gy0;
		data->gy = -imuData.gx0;
		data->gz = -imuData.gz0;
		data->mx = -imuData.my0;
		data->my = -imuData.mx0;
		data->mz = -imuData.mz0;
	}

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

static int kernel_map_gyro_buffer(void)
{
	struct qvr_external_sensor *sensor = &qvr_external_sensor;
	int ret = 0;

	sensor->qvr_buf = dma_buf_get(sensor->fd);
	if (IS_ERR_OR_NULL(sensor->qvr_buf)) {
		ret = -ENOMEM;
		pr_err("dma_buf_get failed for fd: %d\n", sensor->fd);
		goto done;
	}
	ret = dma_buf_begin_cpu_access(sensor->qvr_buf, DMA_BIDIRECTIONAL);
	if (ret) {
		pr_err("%s: dma_buf_begin_cpu_access failed\n", __func__);
		goto err_dma;
	}
	sensor->vsize = sensor->qvr_buf->size;
	sensor->vaddr = dma_buf_kmap(sensor->qvr_buf, 0);
	if (IS_ERR_OR_NULL(sensor->vaddr)) {
		ret = -ENOMEM;
		pr_err("dma_buf_kmap failed for fd: %d\n", sensor->fd);
		goto err_end_access;
	}

	return 0;

err_end_access:
	dma_buf_end_cpu_access(sensor->qvr_buf, DMA_BIDIRECTIONAL);
err_dma:
	dma_buf_put(sensor->qvr_buf);
	sensor->qvr_buf = NULL;
done:
	return ret;

}


static void kernel_unmap_gyro_buffer(void)
{
	struct qvr_external_sensor *sensor = &qvr_external_sensor;

	if (IS_ERR_OR_NULL(sensor->vaddr))
		return;
	dma_buf_kunmap(sensor->qvr_buf, 0, sensor->vaddr);
	dma_buf_end_cpu_access(sensor->qvr_buf, DMA_BIDIRECTIONAL);
	sensor->vaddr = NULL;
	dma_buf_put(sensor->qvr_buf);
	sensor->qvr_buf = NULL;
}

static ssize_t fd_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, 16, "%d\n", qvr_external_sensor.fd);
}

static ssize_t fd_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	struct qvr_external_sensor *sensor = &qvr_external_sensor;
	int ret;

	ret = kstrtoint(buf, 10, &sensor->fd);
	if (ret < 0)
		return ret;
	if (sensor->fd == -1)
		kernel_unmap_gyro_buffer();
	else
		kernel_map_gyro_buffer();
	sensor->ts_base = 0;
	sensor->ts_offset = 0;

	return count;
}

static ssize_t ts_base_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 16, "%lld\n", qvr_external_sensor.ts_base);
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
	return snprintf(buf, 16, "%lld\n", qvr_external_sensor.ts_offset);
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
	struct qvr_external_sensor *sensor = &qvr_external_sensor;
	int ret;
	char *node_name = "qcom,smp2p-interrupt-qvrexternal-5-out";
	sensor->hdev = hdev;

	ret = register_smp2p(&hdev->dev, node_name, &sensor->gpio_info_out);
	if (ret) {
		pr_err("%s: register_smp2p failed\n", __func__);
		goto err_free;
	}
	ret = hid_open_report(hdev);
	if (ret) {
		pr_err("%s: hid_open_report failed\n", __func__);
		goto err_free;
	}
	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		pr_err("%s: hid_hw_start failed\n", __func__);
		goto err_free;
	}
	sensor->device = &hdev->dev;

	return 0;

err_free:
	return ret;

}

static int qvr_external_sensor_fops_open(struct inode *inode,
	struct file *file)
{
	return 0;
}

static int qvr_external_sensor_fops_close(struct inode *inode,
	struct file *file)
{
	return 0;
}
static long qvr_external_sensor_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct qvr_external_sensor *sensor = &qvr_external_sensor;
	struct qvr_calib_data data;
	uint8_t *calib_data;
	void __user *argp = (void __user *)arg;
	int ret;

	if (sensor->device == NULL) {
		pr_err("%s: device not connected\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case QVR_START_IMU:
		ret = control_imu_stream(1);
		return ret;
	case QVR_STOP_IMU:
		ret = control_imu_stream(0);
		return ret;
	case QVR_READ_CALIB_DATA_LEN:
		sensor->calib_data_len = -1;
		ret = read_calibration_len();
		if (ret < 0)
			return ret;
		if (copy_to_user(argp, &sensor->calib_data_len,
				sizeof(sensor->calib_data_len)))
			return -EFAULT;
		return 0;
	case QVR_READ_CALIB_DATA:
		sensor->calib_data_recv = 0;
		calib_data = read_calibration_data();
		if (calib_data == NULL)
			return -ENOMEM;
		data.data_ptr = (__u64)arg;
		if (copy_to_user(u64_to_user_ptr(data.data_ptr), calib_data,
				sensor->calib_data_len)) {
			kfree(calib_data);
			return -EFAULT;
		}
		kfree(calib_data);
		return 0;
	default:
		pr_err("%s: wrong command\n", __func__);
		return -EINVAL;

	}
	return 0;
}
static int qvr_external_sensor_raw_event(struct hid_device *hid,
	struct hid_report *report,
	u8 *data, int size)
{
	struct qvr_external_sensor *sensor = &qvr_external_sensor;
	static int val;
	int ret = -1;

	if (sensor->vaddr != NULL && report->id == 0x1) {
		ret = qvr_send_package_wrap(data/*hid_value*/, size, hid);
		if (ret == 0) {
			val = 1 ^ val;
			qcom_smem_state_update_bits(
				sensor->gpio_info_out.smem_state,
				BIT(sensor->gpio_info_out.smem_bit), val);
			ret = -1;
		}
	}
	if (report->id == 0x2) {
		if (data[0] == 2 && data[1] == 0) /*calibration data len*/
			sensor->calib_data_len = (data[3] << 24)
				| (data[4] << 16) | (data[5] << 8) | data[6];
		else if (data[0] == 2 && data[1] == 1) { /*calibration data*/
			sensor->calib_data_pkt = data;
			sensor->calib_data_recv = 1;
		} else if (data[0] == 2 && data[1] == 4) /*calibration ack*/
			sensor->ext_ack = 1;

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
	{ HID_USB_DEVICE(USB_VENDOR_ID_NREAL, USB_DEVICE_ID_NREAL) },
	{ }
};
MODULE_DEVICE_TABLE(hid, qvr_external_sensor_table);

static const struct file_operations qvr_external_sensor_ops = {
	.owner = THIS_MODULE,
	.open = qvr_external_sensor_fops_open,
	.unlocked_ioctl = qvr_external_sensor_ioctl,
	.compat_ioctl = qvr_external_sensor_ioctl,
	.release = qvr_external_sensor_fops_close,
};

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
	struct qvr_external_sensor *sensor = &qvr_external_sensor;
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

	ret = alloc_chrdev_region(&sensor->dev_no, 0, 1, "qvr_external_sensor");
	if (ret < 0) {
		pr_err("%s: alloc_chrdev_region failed\n");
		return ret;
	}
	cdev_init(&sensor->cdev, &qvr_external_sensor_ops);
	ret = cdev_add(&sensor->cdev, sensor->dev_no, 1);

	if (ret < 0) {
		pr_err("%s: cdev_add failed\n");
		return ret;
	}
	sensor->class = class_create(THIS_MODULE, "qvr_external_sensor");
	if (sensor->class == NULL) {
		cdev_del(&sensor->cdev);
		unregister_chrdev_region(sensor->dev_no, 1);
		return -ret;
	}
	sensor->dev = device_create(sensor->class, NULL,
		MKDEV(MAJOR(sensor->dev_no), 0), NULL,
		"qvr_external_sensor_ioctl");
	if (sensor->dev == NULL) {
		class_destroy(sensor->class);
		cdev_del(&sensor->cdev);
		unregister_chrdev_region(sensor->dev_no, 1);
		return -ret;
	}
	return ret;
}

static void __exit qvr_external_sensor_exit(void)
{
	struct qvr_external_sensor *sensor = &qvr_external_sensor;

	device_destroy(sensor->class, MKDEV(MAJOR(sensor->dev_no), 0));
	class_destroy(sensor->class);
	cdev_del(&sensor->cdev);
	unregister_chrdev_region(sensor->dev_no, 1);
	kobject_put(qvr_external_sensor_kobj);
}

module_init(qvr_external_sensor_init);
module_exit(qvr_external_sensor_exit);
MODULE_LICENSE("GPL v2");
