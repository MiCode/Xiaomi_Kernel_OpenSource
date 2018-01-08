/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012-2015 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2017 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx.h>
#include "synaptics_dsx_core.h"

#define GESTURE_PHYS_NAME "synaptics_dsx/gesture"

#define TUNING_SYSFS_DIR_NAME "tuning"

#define STORE_GESTURES
#ifdef STORE_GESTURES
#define GESTURES_TO_STORE 10
#endif

#define CTRL23_FINGER_REPORT_ENABLE_BIT 0
#define CTRL27_UDG_ENABLE_BIT 4
#define WAKEUP_GESTURE_MODE 0x02

static ssize_t udg_sysfs_engine_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_detection_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_detection_score_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_detection_index_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_registration_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_registration_begin_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_registration_status_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_template_size_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_template_max_index_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_template_detection_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_template_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_template_valid_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_template_valid_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_template_clear_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_trace_size_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_template_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t udg_sysfs_template_data_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t udg_sysfs_trace_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t udg_sysfs_template_displacement_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_template_displacement_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_rotation_invariance_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_rotation_invariance_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_scale_invariance_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_scale_invariance_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_threshold_factor_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_threshold_factor_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_match_metric_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_match_metric_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t udg_sysfs_max_inter_stroke_time_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t udg_sysfs_max_inter_stroke_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static int udg_read_tuning_params(void);

static int udg_write_tuning_params(void);

static int udg_detection_enable(bool enable);

static int udg_engine_enable(bool enable);

static int udg_set_index(unsigned char index);

#ifdef STORE_GESTURES
static int udg_read_valid_data(void);
static int udg_write_valid_data(void);
static int udg_read_template_data(unsigned char index);
static int udg_write_template_data(void);
#endif

enum gesture_type {
	DETECTION = 0x0f,
	REGISTRATION = 0x10,
};

struct udg_tuning {
	union {
		struct {
			unsigned char maximum_number_of_templates;
			unsigned char template_size;
			unsigned char template_disp_lsb;
			unsigned char template_disp_msb;
			unsigned char rotation_inv_lsb;
			unsigned char rotation_inv_msb;
			unsigned char scale_inv_lsb;
			unsigned char scale_inv_msb;
			unsigned char thres_factor_lsb;
			unsigned char thres_factor_msb;
			unsigned char metric_thres_lsb;
			unsigned char metric_thres_msb;
			unsigned char inter_stroke_lsb;
			unsigned char inter_stroke_msb;
		} __packed;
		unsigned char data[14];
	};
};

struct udg_addr {
	unsigned short data_4;
	unsigned short ctrl_18;
	unsigned short ctrl_20;
	unsigned short ctrl_23;
	unsigned short ctrl_27;
	unsigned short ctrl_41;
	unsigned short trace_x;
	unsigned short trace_y;
	unsigned short trace_segment;
	unsigned short template_helper;
	unsigned short template_data;
	unsigned short template_flags;
};

struct synaptics_rmi4_f12_query_0 {
	union {
		struct {
			struct {
				unsigned char has_register_descriptors:1;
				unsigned char has_closed_cover:1;
				unsigned char has_fast_glove_detect:1;
				unsigned char has_dribble:1;
				unsigned char has_4p4_jitter_filter_strength:1;
				unsigned char f12_query0_s0_b5__7:3;
			} __packed;
			struct {
				unsigned char max_num_templates:4;
				unsigned char f12_query0_s1_b4__7:4;
				unsigned char template_size_lsb;
				unsigned char template_size_msb;
			} __packed;
		};
		unsigned char data[4];
	};
};

struct synaptics_rmi4_f12_query_5 {
	union {
		struct {
			unsigned char size_of_query6;
			struct {
				unsigned char ctrl0_is_present:1;
				unsigned char ctrl1_is_present:1;
				unsigned char ctrl2_is_present:1;
				unsigned char ctrl3_is_present:1;
				unsigned char ctrl4_is_present:1;
				unsigned char ctrl5_is_present:1;
				unsigned char ctrl6_is_present:1;
				unsigned char ctrl7_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl8_is_present:1;
				unsigned char ctrl9_is_present:1;
				unsigned char ctrl10_is_present:1;
				unsigned char ctrl11_is_present:1;
				unsigned char ctrl12_is_present:1;
				unsigned char ctrl13_is_present:1;
				unsigned char ctrl14_is_present:1;
				unsigned char ctrl15_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl16_is_present:1;
				unsigned char ctrl17_is_present:1;
				unsigned char ctrl18_is_present:1;
				unsigned char ctrl19_is_present:1;
				unsigned char ctrl20_is_present:1;
				unsigned char ctrl21_is_present:1;
				unsigned char ctrl22_is_present:1;
				unsigned char ctrl23_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl24_is_present:1;
				unsigned char ctrl25_is_present:1;
				unsigned char ctrl26_is_present:1;
				unsigned char ctrl27_is_present:1;
				unsigned char ctrl28_is_present:1;
				unsigned char ctrl29_is_present:1;
				unsigned char ctrl30_is_present:1;
				unsigned char ctrl31_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl32_is_present:1;
				unsigned char ctrl33_is_present:1;
				unsigned char ctrl34_is_present:1;
				unsigned char ctrl35_is_present:1;
				unsigned char ctrl36_is_present:1;
				unsigned char ctrl37_is_present:1;
				unsigned char ctrl38_is_present:1;
				unsigned char ctrl39_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl40_is_present:1;
				unsigned char ctrl41_is_present:1;
				unsigned char ctrl42_is_present:1;
				unsigned char ctrl43_is_present:1;
				unsigned char ctrl44_is_present:1;
				unsigned char ctrl45_is_present:1;
				unsigned char ctrl46_is_present:1;
				unsigned char ctrl47_is_present:1;
			} __packed;
		};
		unsigned char data[7];
	};
};

struct synaptics_rmi4_f12_query_8 {
	union {
		struct {
			unsigned char size_of_query9;
			struct {
				unsigned char data0_is_present:1;
				unsigned char data1_is_present:1;
				unsigned char data2_is_present:1;
				unsigned char data3_is_present:1;
				unsigned char data4_is_present:1;
				unsigned char data5_is_present:1;
				unsigned char data6_is_present:1;
				unsigned char data7_is_present:1;
			} __packed;
			struct {
				unsigned char data8_is_present:1;
				unsigned char data9_is_present:1;
				unsigned char data10_is_present:1;
				unsigned char data11_is_present:1;
				unsigned char data12_is_present:1;
				unsigned char data13_is_present:1;
				unsigned char data14_is_present:1;
				unsigned char data15_is_present:1;
			} __packed;
			struct {
				unsigned char data16_is_present:1;
				unsigned char data17_is_present:1;
				unsigned char data18_is_present:1;
				unsigned char data19_is_present:1;
				unsigned char data20_is_present:1;
				unsigned char data21_is_present:1;
				unsigned char data22_is_present:1;
				unsigned char data23_is_present:1;
			} __packed;
		};
		unsigned char data[4];
	};
};

struct synaptics_rmi4_f12_control_41 {
	union {
		struct {
			unsigned char enable_registration:1;
			unsigned char template_index:4;
			unsigned char begin:1;
			unsigned char f12_ctrl41_b6__7:2;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_udg_handle {
	atomic_t attn_event;
	unsigned char intr_mask;
	unsigned char report_flags;
	unsigned char object_type_enable1;
	unsigned char object_type_enable2;
	unsigned char trace_size;
	unsigned char template_index;
	unsigned char max_num_templates;
	unsigned char detection_score;
	unsigned char detection_index;
	unsigned char detection_status;
	unsigned char registration_status;
	unsigned char *ctrl_buf;
	unsigned char *trace_data_buf;
	unsigned char *template_data_buf;
#ifdef STORE_GESTURES
	unsigned char gestures_to_store;
	unsigned char *storage_buf;
	unsigned char valid_buf[2];
#endif
	unsigned short trace_data_buf_size;
	unsigned short template_size;
	unsigned short template_data_size;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	unsigned short ctrl_18_sub10_off;
	unsigned short ctrl_20_sub1_off;
	unsigned short ctrl_23_sub3_off;
	unsigned short ctrl_27_sub5_off;
	struct input_dev *udg_dev;
	struct kobject *tuning_dir;
	struct udg_addr addr;
	struct udg_tuning tuning;
	struct synaptics_rmi4_data *rmi4_data;
};

static struct device_attribute attrs[] = {
	__ATTR(engine_enable, S_IWUSR,
			synaptics_rmi4_show_error,
			udg_sysfs_engine_enable_store),
	__ATTR(detection_enable, S_IWUSR,
			synaptics_rmi4_show_error,
			udg_sysfs_detection_enable_store),
	__ATTR(detection_score, S_IRUGO,
			udg_sysfs_detection_score_show,
			synaptics_rmi4_store_error),
	__ATTR(detection_index, S_IRUGO,
			udg_sysfs_detection_index_show,
			synaptics_rmi4_store_error),
	__ATTR(registration_enable, S_IWUSR,
			synaptics_rmi4_show_error,
			udg_sysfs_registration_enable_store),
	__ATTR(registration_begin, S_IWUSR,
			synaptics_rmi4_show_error,
			udg_sysfs_registration_begin_store),
	__ATTR(registration_status, S_IRUGO,
			udg_sysfs_registration_status_show,
			synaptics_rmi4_store_error),
	__ATTR(template_size, S_IRUGO,
			udg_sysfs_template_size_show,
			synaptics_rmi4_store_error),
	__ATTR(template_max_index, S_IRUGO,
			udg_sysfs_template_max_index_show,
			synaptics_rmi4_store_error),
	__ATTR(template_detection, S_IRUGO,
			udg_sysfs_template_detection_show,
			synaptics_rmi4_store_error),
	__ATTR(template_index, S_IWUSR,
			synaptics_rmi4_show_error,
			udg_sysfs_template_index_store),
	__ATTR(template_valid, (S_IRUGO | S_IWUSR),
			udg_sysfs_template_valid_show,
			udg_sysfs_template_valid_store),
	__ATTR(template_clear, S_IWUSR,
			synaptics_rmi4_show_error,
			udg_sysfs_template_clear_store),
	__ATTR(trace_size, S_IRUGO,
			udg_sysfs_trace_size_show,
			synaptics_rmi4_store_error),
};

static struct bin_attribute template_data = {
	.attr = {
		.name = "template_data",
		.mode = (S_IRUGO | S_IWUSR),
	},
	.size = 0,
	.read = udg_sysfs_template_data_show,
	.write = udg_sysfs_template_data_store,
};

static struct bin_attribute trace_data = {
	.attr = {
		.name = "trace_data",
		.mode = S_IRUGO,
	},
	.size = 0,
	.read = udg_sysfs_trace_data_show,
	.write = NULL,
};

static struct device_attribute params[] = {
	__ATTR(template_displacement, (S_IRUGO | S_IWUSR),
			udg_sysfs_template_displacement_show,
			udg_sysfs_template_displacement_store),
	__ATTR(rotation_invariance, (S_IRUGO | S_IWUSR),
			udg_sysfs_rotation_invariance_show,
			udg_sysfs_rotation_invariance_store),
	__ATTR(scale_invariance, (S_IRUGO | S_IWUSR),
			udg_sysfs_scale_invariance_show,
			udg_sysfs_scale_invariance_store),
	__ATTR(threshold_factor, (S_IRUGO | S_IWUSR),
			udg_sysfs_threshold_factor_show,
			udg_sysfs_threshold_factor_store),
	__ATTR(match_metric_threshold, (S_IRUGO | S_IWUSR),
			udg_sysfs_match_metric_threshold_show,
			udg_sysfs_match_metric_threshold_store),
	__ATTR(max_inter_stroke_time, (S_IRUGO | S_IWUSR),
			udg_sysfs_max_inter_stroke_time_show,
			udg_sysfs_max_inter_stroke_time_store),
};

static struct synaptics_rmi4_udg_handle *udg;

static unsigned char ctrl_18_sub_size[] = {10, 10, 10, 2, 3, 4, 3, 3, 1, 1};
static unsigned char ctrl_20_sub_size[] = {2};
static unsigned char ctrl_23_sub_size[] = {1, 1, 1};
static unsigned char ctrl_27_sub_size[] = {1, 5, 2, 1, 7};

DECLARE_COMPLETION(udg_remove_complete);

static ssize_t udg_sysfs_engine_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	bool enable;
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 1)
		enable = true;
	else if (input == 0)
		enable = false;
	else
		return -EINVAL;

	retval = udg_engine_enable(enable);
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t udg_sysfs_detection_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	bool enable;
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 1)
		enable = true;
	else if (input == 0)
		enable = false;
	else
		return -EINVAL;

	udg->detection_status = 0;

	retval = udg_detection_enable(enable);
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t udg_sysfs_detection_score_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", udg->detection_score);
}

static ssize_t udg_sysfs_detection_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", udg->detection_index);
}

static ssize_t udg_sysfs_registration_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	bool enable;
	unsigned int input;
	struct synaptics_rmi4_f12_control_41 control_41;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 1)
		enable = true;
	else if (input == 0)
		enable = false;
	else
		return -EINVAL;

	if (enable) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				udg->addr.ctrl_23,
				udg->ctrl_buf,
				udg->ctrl_23_sub3_off + 1);
		if (retval < 0)
			return retval;

		udg->ctrl_buf[0] = 0;
		udg->ctrl_buf[0] |= (1 << CTRL23_FINGER_REPORT_ENABLE_BIT);
		if (udg->ctrl_23_sub3_off)
			udg->ctrl_buf[udg->ctrl_23_sub3_off] = 0;

		retval = synaptics_rmi4_reg_write(rmi4_data,
				udg->addr.ctrl_23,
				udg->ctrl_buf,
				udg->ctrl_23_sub3_off + 1);
		if (retval < 0)
			return retval;
	} else {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				udg->addr.ctrl_23,
				udg->ctrl_buf,
				udg->ctrl_23_sub3_off + 1);
		if (retval < 0)
			return retval;

		udg->ctrl_buf[0] = udg->object_type_enable1;
		if (udg->ctrl_23_sub3_off) {
			udg->ctrl_buf[udg->ctrl_23_sub3_off] =
					udg->object_type_enable2;
		}

		retval = synaptics_rmi4_reg_write(rmi4_data,
				udg->addr.ctrl_23,
				udg->ctrl_buf,
				udg->ctrl_23_sub3_off + 1);
		if (retval < 0)
			return retval;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.ctrl_41,
			control_41.data,
			sizeof(control_41.data));
	if (retval < 0)
		return retval;

	control_41.enable_registration = enable ? 1 : 0;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			udg->addr.ctrl_41,
			control_41.data,
			sizeof(control_41.data));
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t udg_sysfs_registration_begin_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	bool begin;
	unsigned int input;
	struct synaptics_rmi4_f12_control_41 control_41;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 1)
		begin = true;
	else if (input == 0)
		begin = false;
	else
		return -EINVAL;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.ctrl_41,
			control_41.data,
			sizeof(control_41.data));
	if (retval < 0)
		return retval;

	control_41.begin = begin ? 1 : 0;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			udg->addr.ctrl_41,
			control_41.data,
			sizeof(control_41.data));
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t udg_sysfs_registration_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", udg->registration_status);
}

static ssize_t udg_sysfs_template_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", udg->template_size);
}

static ssize_t udg_sysfs_template_max_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", udg->max_num_templates - 1);
}

static ssize_t udg_sysfs_template_detection_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	int attn_event;
	unsigned char detection_status;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	attn_event = atomic_read(&udg->attn_event);
	atomic_set(&udg->attn_event, 0);

	if (attn_event == 0)
		return snprintf(buf, PAGE_SIZE, "0\n");

	if (udg->detection_status == 0) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				udg->addr.data_4,
				rmi4_data->gesture_detection,
				sizeof(rmi4_data->gesture_detection));
		if (retval < 0)
			return retval;

		udg->detection_status = rmi4_data->gesture_detection[0];
	}

	detection_status = udg->detection_status;
	udg->detection_status = 0;

	switch (detection_status) {
	case DETECTION:
		udg->detection_score = rmi4_data->gesture_detection[1];
		udg->detection_index = rmi4_data->gesture_detection[4];
		udg->trace_size = rmi4_data->gesture_detection[3];
		break;
	case REGISTRATION:
		udg->registration_status = rmi4_data->gesture_detection[1];
		udg->trace_size = rmi4_data->gesture_detection[3];
		break;
	default:
		return snprintf(buf, PAGE_SIZE, "0\n");
	}

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", detection_status);
}

static ssize_t udg_sysfs_template_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long index;

	retval = sstrtoul(buf, 10, &index);
	if (retval)
		return retval;

	retval = udg_set_index((unsigned char)index);
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t udg_sysfs_template_valid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char valid;
	unsigned char offset;
	unsigned char byte_num;
	unsigned char template_flags[2];
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	byte_num = udg->template_index / 8;
	offset = udg->template_index % 8;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.template_flags,
			template_flags,
			sizeof(template_flags));
	if (retval < 0)
		return retval;

	valid = (template_flags[byte_num] & (1 << offset)) >> offset;

	return snprintf(buf, PAGE_SIZE, "%u\n", valid);
}

static ssize_t udg_sysfs_template_valid_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long valid;
	unsigned char offset;
	unsigned char byte_num;
	unsigned char template_flags[2];
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	retval = sstrtoul(buf, 10, &valid);
	if (retval)
		return retval;

	if (valid > 0)
		valid = 1;

	byte_num = udg->template_index / 8;
	offset = udg->template_index % 8;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.template_flags,
			template_flags,
			sizeof(template_flags));
	if (retval < 0)
		return retval;

	if (valid)
		template_flags[byte_num] |= (1 << offset);
	else
		template_flags[byte_num] &= ~(1 << offset);

	retval = synaptics_rmi4_reg_write(rmi4_data,
			udg->addr.template_flags,
			template_flags,
			sizeof(template_flags));
	if (retval < 0)
		return retval;

#ifdef STORE_GESTURES
	udg_read_valid_data();
#endif

	return count;
}

static ssize_t udg_sysfs_template_clear_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	const char cmd[] = {'0', 0};
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	memset(udg->template_data_buf, 0x00, udg->template_data_size);

	retval = synaptics_rmi4_reg_write(rmi4_data,
			udg->addr.template_data,
			udg->template_data_buf,
			udg->template_data_size);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to clear template data\n",
				__func__);
		return retval;
	}

	retval = udg_sysfs_template_valid_store(dev, attr, cmd, 1);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to clear valid bit\n",
				__func__);
		return retval;
	}

#ifdef STORE_GESTURES
	udg_read_template_data(udg->template_index);
	udg_read_valid_data();
#endif

	return count;
}

static ssize_t udg_sysfs_trace_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", udg->trace_size);
}

static ssize_t udg_sysfs_trace_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned short index = 0;
	unsigned short trace_data_size;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	trace_data_size = udg->trace_size * 5;

	if (trace_data_size == 0)
		return -EINVAL;

	if (count < trace_data_size) {
/*		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Not enough space (%d bytes) in buffer\n",
				__func__, count);
				*/
		return -EINVAL;
	}

	if (udg->trace_data_buf_size < trace_data_size) {
		if (udg->trace_data_buf_size)
			kfree(udg->trace_data_buf);
		udg->trace_data_buf = kzalloc(trace_data_size, GFP_KERNEL);
		if (!udg->trace_data_buf) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to alloc mem for trace data buffer\n",
					__func__);
			udg->trace_data_buf_size = 0;
			return -ENOMEM;
		}
		udg->trace_data_buf_size = trace_data_size;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.trace_x,
			&udg->trace_data_buf[index],
			udg->trace_size * 2);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read trace X data\n",
				__func__);
		return retval;
	} else {
		index += udg->trace_size * 2;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.trace_y,
			&udg->trace_data_buf[index],
			udg->trace_size * 2);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read trace Y data\n",
				__func__);
		return retval;
	} else {
		index += udg->trace_size * 2;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.trace_segment,
			&udg->trace_data_buf[index],
			udg->trace_size);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read trace segment data\n",
				__func__);
		return retval;
	}

	retval = secure_memcpy(buf, count, udg->trace_data_buf,
			udg->trace_data_buf_size, trace_data_size);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to copy trace data\n",
				__func__);
		return retval;
	}

	return trace_data_size;
}

static ssize_t udg_sysfs_template_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	if (count < udg->template_data_size) {
/*	dev_err(rmi4_data->pdev->dev.parent,
				"%s: Not enough space (%d bytes) in buffer\n",
				__func__, count);
		return -EINVAL;
		*/
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.template_data,
			udg->template_data_buf,
			udg->template_data_size);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read template data\n",
				__func__);
		return retval;
	}

	retval = secure_memcpy(buf, count, udg->template_data_buf,
			udg->template_data_size, udg->template_data_size);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to copy template data\n",
				__func__);
		return retval;
	}

#ifdef STORE_GESTURES
	udg_read_template_data(udg->template_index);
	udg_read_valid_data();
#endif

	return udg->template_data_size;
}

static ssize_t udg_sysfs_template_data_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	retval = secure_memcpy(udg->template_data_buf, udg->template_data_size,
			buf, count, count);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to copy template data\n",
				__func__);
		return retval;
	}

	retval = synaptics_rmi4_reg_write(rmi4_data,
			udg->addr.template_data,
			udg->template_data_buf,
			count);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write template data\n",
				__func__);
		return retval;
	}

#ifdef STORE_GESTURES
	udg_read_template_data(udg->template_index);
	udg_read_valid_data();
#endif

	return count;
}

static ssize_t udg_sysfs_template_displacement_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned short template_displacement;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	template_displacement =
			((unsigned short)udg->tuning.template_disp_lsb << 0) |
			((unsigned short)udg->tuning.template_disp_msb << 8);

	return snprintf(buf, PAGE_SIZE, "%u\n", template_displacement);
}

static ssize_t udg_sysfs_template_displacement_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long input;

	retval = sstrtoul(buf, 10, &input);
	if (retval)
		return retval;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	udg->tuning.template_disp_lsb = (unsigned char)(input >> 0);
	udg->tuning.template_disp_msb = (unsigned char)(input >> 8);

	retval = udg_write_tuning_params();
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t udg_sysfs_rotation_invariance_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned short rotation_invariance;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	rotation_invariance =
			((unsigned short)udg->tuning.rotation_inv_lsb << 0) |
			((unsigned short)udg->tuning.rotation_inv_msb << 8);

	return snprintf(buf, PAGE_SIZE, "%u\n", rotation_invariance);
}

static ssize_t udg_sysfs_rotation_invariance_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long input;

	retval = sstrtoul(buf, 10, &input);
	if (retval)
		return retval;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	udg->tuning.rotation_inv_lsb = (unsigned char)(input >> 0);
	udg->tuning.rotation_inv_msb = (unsigned char)(input >> 8);

	retval = udg_write_tuning_params();
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t udg_sysfs_scale_invariance_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned short scale_invariance;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	scale_invariance =
			((unsigned short)udg->tuning.scale_inv_lsb << 0) |
			((unsigned short)udg->tuning.scale_inv_msb << 8);

	return snprintf(buf, PAGE_SIZE, "%u\n", scale_invariance);
}

static ssize_t udg_sysfs_scale_invariance_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long input;

	retval = sstrtoul(buf, 10, &input);
	if (retval)
		return retval;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	udg->tuning.scale_inv_lsb = (unsigned char)(input >> 0);
	udg->tuning.scale_inv_msb = (unsigned char)(input >> 8);

	retval = udg_write_tuning_params();
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t udg_sysfs_threshold_factor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned short threshold_factor;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	threshold_factor =
			((unsigned short)udg->tuning.thres_factor_lsb << 0) |
			((unsigned short)udg->tuning.thres_factor_msb << 8);

	return snprintf(buf, PAGE_SIZE, "%u\n", threshold_factor);
}

static ssize_t udg_sysfs_threshold_factor_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long input;

	retval = sstrtoul(buf, 10, &input);
	if (retval)
		return retval;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	udg->tuning.thres_factor_lsb = (unsigned char)(input >> 0);
	udg->tuning.thres_factor_msb = (unsigned char)(input >> 8);

	retval = udg_write_tuning_params();
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t udg_sysfs_match_metric_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned short match_metric_threshold;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	match_metric_threshold =
			((unsigned short)udg->tuning.metric_thres_lsb << 0) |
			((unsigned short)udg->tuning.metric_thres_msb << 8);

	return snprintf(buf, PAGE_SIZE, "%u\n", match_metric_threshold);
}

static ssize_t udg_sysfs_match_metric_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long input;

	retval = sstrtoul(buf, 10, &input);
	if (retval)
		return retval;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	udg->tuning.metric_thres_lsb = (unsigned char)(input >> 0);
	udg->tuning.metric_thres_msb = (unsigned char)(input >> 8);

	retval = udg_write_tuning_params();
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t udg_sysfs_max_inter_stroke_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned short max_inter_stroke_time;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	max_inter_stroke_time =
			((unsigned short)udg->tuning.inter_stroke_lsb << 0) |
			((unsigned short)udg->tuning.inter_stroke_msb << 8);

	return snprintf(buf, PAGE_SIZE, "%u\n", max_inter_stroke_time);
}

static ssize_t udg_sysfs_max_inter_stroke_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long input;

	retval = sstrtoul(buf, 10, &input);
	if (retval)
		return retval;

	retval = udg_read_tuning_params();
	if (retval < 0)
		return retval;

	udg->tuning.inter_stroke_lsb = (unsigned char)(input >> 0);
	udg->tuning.inter_stroke_msb = (unsigned char)(input >> 8);

	retval = udg_write_tuning_params();
	if (retval < 0)
		return retval;

	return count;
}

static int udg_ctrl_subpacket(unsigned char ctrlreg,
		unsigned char subpacket,
		struct synaptics_rmi4_f12_query_5 *query_5)
{
	int retval;
	unsigned char cnt;
	unsigned char regnum;
	unsigned char bitnum;
	unsigned char q5_index;
	unsigned char q6_index;
	unsigned char offset;
	unsigned char max_ctrlreg;
	unsigned char *query_6;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	max_ctrlreg = (sizeof(query_5->data) - 1) * 8 - 1;

	if (ctrlreg > max_ctrlreg) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Control register number (%d) over limit\n",
				__func__, ctrlreg);
		return -EINVAL;
	}

	q5_index = ctrlreg / 8 + 1;
	bitnum = ctrlreg % 8;
	if ((query_5->data[q5_index] & (1 << bitnum)) == 0x00) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Control %d is not present\n",
				__func__, ctrlreg);
		return -EINVAL;
	}

	query_6 = kmalloc(query_5->size_of_query6, GFP_KERNEL);
	if (!query_6) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for query 6\n",
				__func__);
		return -ENOMEM;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->query_base_addr + 6,
			query_6,
			query_5->size_of_query6);
	if (retval < 0)
		goto exit;

	q6_index = 0;

	for (regnum = 0; regnum < ctrlreg; regnum++) {
		q5_index = regnum / 8 + 1;
		bitnum = regnum % 8;
		if ((query_5->data[q5_index] & (1 << bitnum)) == 0x00)
			continue;

		if (query_6[q6_index] == 0x00)
			q6_index += 3;
		else
			q6_index++;

		while (query_6[q6_index] & ~MASK_7BIT)
			q6_index++;

		q6_index++;
	}

	cnt = 0;
	q6_index++;
	offset = subpacket / 7;
	bitnum = subpacket % 7;

	do {
		if (cnt == offset) {
			if (query_6[q6_index + cnt] & (1 << bitnum))
				retval = 1;
			else
				retval = 0;
			goto exit;
		}
		cnt++;
	} while (query_6[q6_index + cnt - 1] & ~MASK_7BIT);

	retval = 0;

exit:
	kfree(query_6);

	return retval;
}

static int udg_read_tuning_params(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.ctrl_18,
			udg->ctrl_buf,
			udg->ctrl_18_sub10_off + sizeof(struct udg_tuning));
	if (retval < 0)
		return retval;

	secure_memcpy(udg->tuning.data,
			sizeof(udg->tuning.data),
			(unsigned char *)&udg->ctrl_buf[udg->ctrl_18_sub10_off],
			sizeof(struct udg_tuning),
			sizeof(struct udg_tuning));

	return 0;
}

static int udg_write_tuning_params(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	secure_memcpy((unsigned char *)&udg->ctrl_buf[udg->ctrl_18_sub10_off],
			sizeof(struct udg_tuning),
			udg->tuning.data,
			sizeof(udg->tuning.data),
			sizeof(struct udg_tuning));

	retval = synaptics_rmi4_reg_write(rmi4_data,
			udg->addr.ctrl_18,
			udg->ctrl_buf,
			udg->ctrl_18_sub10_off + sizeof(struct udg_tuning));
	if (retval < 0)
		return retval;

	return 0;
}

static int udg_detection_enable(bool enable)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.ctrl_20,
			udg->ctrl_buf,
			udg->ctrl_20_sub1_off + 1);
	if (retval < 0)
		return retval;

	if (enable)
		udg->ctrl_buf[udg->ctrl_20_sub1_off] = WAKEUP_GESTURE_MODE;
	else
		udg->ctrl_buf[udg->ctrl_20_sub1_off] = udg->report_flags;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			udg->addr.ctrl_20,
			udg->ctrl_buf,
			udg->ctrl_20_sub1_off + 1);
	if (retval < 0)
		return retval;

	return 0;
}

static int udg_engine_enable(bool enable)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	if (enable) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				udg->addr.ctrl_27,
				udg->ctrl_buf,
				udg->ctrl_27_sub5_off + 1);
		if (retval < 0)
			return retval;

		udg->ctrl_buf[udg->ctrl_27_sub5_off] |=
				(1 << CTRL27_UDG_ENABLE_BIT);

		retval = synaptics_rmi4_reg_write(rmi4_data,
				udg->addr.ctrl_27,
				udg->ctrl_buf,
				udg->ctrl_27_sub5_off + 1);
		if (retval < 0)
			return retval;
	} else {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				udg->addr.ctrl_27,
				udg->ctrl_buf,
				udg->ctrl_27_sub5_off + 1);
		if (retval < 0)
			return retval;

		udg->ctrl_buf[udg->ctrl_27_sub5_off] &=
				~(1 << CTRL27_UDG_ENABLE_BIT);

		retval = synaptics_rmi4_reg_write(rmi4_data,
				udg->addr.ctrl_27,
				udg->ctrl_buf,
				udg->ctrl_27_sub5_off + 1);
		if (retval < 0)
			return retval;
	}

	return 0;
}

static void udg_report(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	atomic_set(&udg->attn_event, 1);

	if (rmi4_data->suspend) {
		if (rmi4_data->gesture_detection[0] == 0) {
			retval = synaptics_rmi4_reg_read(rmi4_data,
					udg->addr.data_4,
					rmi4_data->gesture_detection,
					sizeof(rmi4_data->gesture_detection));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to read gesture detection\n",
						__func__);
				return;
			}
		}

		udg->detection_status = rmi4_data->gesture_detection[0];
		rmi4_data->gesture_detection[0] = 0;

		if (udg->detection_status == DETECTION) {
			input_report_key(udg->udg_dev, KEY_WAKEUP, 1);
			input_sync(udg->udg_dev);
			input_report_key(udg->udg_dev, KEY_WAKEUP, 0);
			input_sync(udg->udg_dev);
			rmi4_data->suspend = false;
		}
	}

	return;
}

static int udg_set_index(unsigned char index)
{
	int retval;
	struct synaptics_rmi4_f12_control_41 control_41;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	if (index >= udg->max_num_templates)
		return -EINVAL;

	udg->template_index = index;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.ctrl_41,
			control_41.data,
			sizeof(control_41.data));
	if (retval < 0)
		return retval;

	control_41.template_index = udg->template_index;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			udg->addr.ctrl_41,
			control_41.data,
			sizeof(control_41.data));
	if (retval < 0)
		return retval;

	return 0;
}

#ifdef STORE_GESTURES
static int udg_read_valid_data(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.template_flags,
			udg->valid_buf,
			sizeof(udg->valid_buf));
	if (retval < 0)
		return retval;

	return 0;
}

static int udg_write_valid_data(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			udg->addr.template_flags,
			udg->valid_buf,
			sizeof(udg->valid_buf));
	if (retval < 0)
		return retval;

	return 0;
}

static int udg_read_template_data(unsigned char index)
{
	int retval;
	unsigned char *storage;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	udg_set_index(index);
	storage = &(udg->storage_buf[index * udg->template_data_size]);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.template_data,
			storage,
			udg->template_data_size);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read template data\n",
				__func__);
		return retval;
	}

	return 0;
}

static int udg_write_template_data(void)
{
	int retval;
	unsigned char ii;
	unsigned char *storage;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	for (ii = 0; ii < udg->gestures_to_store; ii++) {
		udg_set_index(ii);
		storage = &(udg->storage_buf[ii * udg->template_data_size]);

		retval = synaptics_rmi4_reg_write(rmi4_data,
				udg->addr.template_data,
				storage,
				udg->template_data_size);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to write template data\n",
					__func__);
			return retval;
		}
	}

	return 0;
}
#endif

static int udg_reg_init(void)
{
	int retval;
	unsigned char ii;
	unsigned char data_offset;
	unsigned char size_of_query;
	unsigned char ctrl_18_offset;
	unsigned char ctrl_20_offset;
	unsigned char ctrl_23_offset;
	unsigned char ctrl_27_offset;
	unsigned char ctrl_41_offset;
	struct synaptics_rmi4_f12_query_0 query_0;
	struct synaptics_rmi4_f12_query_5 query_5;
	struct synaptics_rmi4_f12_query_8 query_8;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->query_base_addr + 7,
			&size_of_query,
			sizeof(size_of_query));
	if (retval < 0)
		return retval;

	if (size_of_query < 4) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: User defined gesture support unavailable "
				"(missing data registers)\n",
				__func__);
		retval = -ENODEV;
		return retval;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->query_base_addr + 8,
			query_8.data,
			sizeof(query_8.data));
	if (retval < 0)
		return retval;

	if ((query_8.data16_is_present) &&
			(query_8.data17_is_present) &&
			(query_8.data18_is_present) &&
			(query_8.data19_is_present) &&
			(query_8.data20_is_present) &&
			(query_8.data21_is_present)) {
		data_offset = query_8.data0_is_present +
				query_8.data1_is_present +
				query_8.data2_is_present +
				query_8.data3_is_present;
		udg->addr.data_4 = udg->data_base_addr + data_offset;
		data_offset = data_offset +
				query_8.data4_is_present +
				query_8.data5_is_present +
				query_8.data6_is_present +
				query_8.data7_is_present +
				query_8.data8_is_present +
				query_8.data9_is_present +
				query_8.data10_is_present +
				query_8.data11_is_present +
				query_8.data12_is_present +
				query_8.data13_is_present +
				query_8.data14_is_present +
				query_8.data15_is_present;
		udg->addr.trace_x = udg->data_base_addr + data_offset;
		udg->addr.trace_y = udg->addr.trace_x + 1;
		udg->addr.trace_segment = udg->addr.trace_y + 1;
		udg->addr.template_helper = udg->addr.trace_segment + 1;
		udg->addr.template_data = udg->addr.template_helper + 1;
		udg->addr.template_flags = udg->addr.template_data + 1;
	} else {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: User defined gesture support unavailable "
				"(missing data registers)\n",
				__func__);
		retval = -ENODEV;
		return retval;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->query_base_addr + 4,
			&size_of_query,
			sizeof(size_of_query));
	if (retval < 0)
		return retval;

	if (size_of_query < 7) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: User defined gesture support unavailable "
				"(missing control registers)\n",
				__func__);
		retval = -ENODEV;
		return retval;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->query_base_addr + 5,
			query_5.data,
			sizeof(query_5.data));
	if (retval < 0)
		return retval;

	ctrl_18_offset = query_5.ctrl0_is_present +
			query_5.ctrl1_is_present +
			query_5.ctrl2_is_present +
			query_5.ctrl3_is_present +
			query_5.ctrl4_is_present +
			query_5.ctrl5_is_present +
			query_5.ctrl6_is_present +
			query_5.ctrl7_is_present +
			query_5.ctrl8_is_present +
			query_5.ctrl9_is_present +
			query_5.ctrl10_is_present +
			query_5.ctrl11_is_present +
			query_5.ctrl12_is_present +
			query_5.ctrl13_is_present +
			query_5.ctrl14_is_present +
			query_5.ctrl15_is_present +
			query_5.ctrl16_is_present +
			query_5.ctrl17_is_present;

	ctrl_20_offset = ctrl_18_offset +
			query_5.ctrl18_is_present +
			query_5.ctrl19_is_present;

	ctrl_23_offset = ctrl_20_offset +
			query_5.ctrl20_is_present +
			query_5.ctrl21_is_present +
			query_5.ctrl22_is_present;

	ctrl_27_offset = ctrl_23_offset+
			query_5.ctrl23_is_present +
			query_5.ctrl24_is_present +
			query_5.ctrl25_is_present +
			query_5.ctrl26_is_present;

	ctrl_41_offset = ctrl_27_offset+
			query_5.ctrl27_is_present +
			query_5.ctrl28_is_present +
			query_5.ctrl29_is_present +
			query_5.ctrl30_is_present +
			query_5.ctrl31_is_present +
			query_5.ctrl32_is_present +
			query_5.ctrl33_is_present +
			query_5.ctrl34_is_present +
			query_5.ctrl35_is_present +
			query_5.ctrl36_is_present +
			query_5.ctrl37_is_present +
			query_5.ctrl38_is_present +
			query_5.ctrl39_is_present +
			query_5.ctrl40_is_present;

	udg->addr.ctrl_18 = udg->control_base_addr + ctrl_18_offset;
	udg->addr.ctrl_20 = udg->control_base_addr + ctrl_20_offset;
	udg->addr.ctrl_23 = udg->control_base_addr + ctrl_23_offset;
	udg->addr.ctrl_27 = udg->control_base_addr + ctrl_27_offset;
	udg->addr.ctrl_41 = udg->control_base_addr + ctrl_41_offset;

	udg->ctrl_18_sub10_off = 0;
	for (ii = 0; ii < 10; ii++) {
		retval = udg_ctrl_subpacket(18, ii, &query_5);
		if (retval == 1)
			udg->ctrl_18_sub10_off += ctrl_18_sub_size[ii];
		else if (retval < 0)
			return retval;
	}

	udg->ctrl_20_sub1_off = 0;
	for (ii = 0; ii < 1; ii++) {
		retval = udg_ctrl_subpacket(20, ii, &query_5);
		if (retval == 1)
			udg->ctrl_20_sub1_off += ctrl_20_sub_size[ii];
		else if (retval < 0)
			return retval;
	}

	udg->ctrl_23_sub3_off = 0;
	for (ii = 0; ii < 3; ii++) {
		retval = udg_ctrl_subpacket(23, ii, &query_5);
		if (retval == 1)
			udg->ctrl_23_sub3_off += ctrl_23_sub_size[ii];
		else if (retval < 0)
			return retval;
	}

	retval = udg_ctrl_subpacket(23, 3, &query_5);
	if (retval == 0)
		udg->ctrl_23_sub3_off = 0;
	else if (retval < 0)
		return retval;

	udg->ctrl_27_sub5_off = 0;
	for (ii = 0; ii < 5; ii++) {
		retval = udg_ctrl_subpacket(27, ii, &query_5);
		if (retval == 1)
			udg->ctrl_27_sub5_off += ctrl_27_sub_size[ii];
		else if (retval < 0)
			return retval;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->query_base_addr + 0,
			query_0.data,
			sizeof(query_0.data));
	if (retval < 0)
		return retval;

	udg->max_num_templates = query_0.max_num_templates;
	udg->template_size =
			((unsigned short)query_0.template_size_lsb << 0) |
			((unsigned short)query_0.template_size_msb << 8);
	udg->template_data_size = udg->template_size * 4 * 2 + 4 + 1;

#ifdef STORE_GESTURES
	udg->gestures_to_store = udg->max_num_templates;
	if (GESTURES_TO_STORE < udg->gestures_to_store)
		udg->gestures_to_store = GESTURES_TO_STORE;
#endif

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.ctrl_20,
			udg->ctrl_buf,
			udg->ctrl_20_sub1_off + 1);
	if (retval < 0)
		return retval;

	udg->report_flags = udg->ctrl_buf[udg->ctrl_20_sub1_off];

	retval = synaptics_rmi4_reg_read(rmi4_data,
			udg->addr.ctrl_23,
			udg->ctrl_buf,
			udg->ctrl_23_sub3_off + 1);
	if (retval < 0)
		return retval;

	udg->object_type_enable1 = udg->ctrl_buf[0];
	if (udg->ctrl_23_sub3_off)
		udg->object_type_enable2 = udg->ctrl_buf[udg->ctrl_23_sub3_off];

	return retval;
}

static int udg_scan_pdt(void)
{
	int retval;
	unsigned char ii;
	unsigned char page;
	unsigned char intr_count = 0;
	unsigned char intr_off;
	unsigned char intr_src;
	unsigned short addr;
	struct synaptics_rmi4_fn_desc fd;
	struct synaptics_rmi4_data *rmi4_data = udg->rmi4_data;

	for (page = 0; page < PAGES_TO_SERVICE; page++) {
		for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
			addr |= (page << 8);

			retval = synaptics_rmi4_reg_read(rmi4_data,
					addr,
					(unsigned char *)&fd,
					sizeof(fd));
			if (retval < 0)
				return retval;

			addr &= ~(MASK_8BIT << 8);

			if (fd.fn_number) {
				dev_dbg(rmi4_data->pdev->dev.parent,
						"%s: Found F%02x\n",
						__func__, fd.fn_number);
				switch (fd.fn_number) {
				case SYNAPTICS_RMI4_F12:
					goto f12_found;
					break;
				}
			} else {
				break;
			}

			intr_count += fd.intr_src_count;
		}
	}

	dev_err(rmi4_data->pdev->dev.parent,
			"%s: Failed to find F12\n",
			__func__);
	return -EINVAL;

f12_found:
	udg->query_base_addr = fd.query_base_addr | (page << 8);
	udg->control_base_addr = fd.ctrl_base_addr | (page << 8);
	udg->data_base_addr = fd.data_base_addr | (page << 8);
	udg->command_base_addr = fd.cmd_base_addr | (page << 8);

	retval = udg_reg_init();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to initialize user defined gesture registers\n",
				__func__);
		return retval;
	}

	udg->intr_mask = 0;
	intr_src = fd.intr_src_count;
	intr_off = intr_count % 8;
	for (ii = intr_off;
			ii < (intr_src + intr_off);
			ii++) {
		udg->intr_mask |= 1 << ii;
	}

	rmi4_data->intr_mask[0] |= udg->intr_mask;

	addr = rmi4_data->f01_ctrl_base_addr + 1;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			addr,
			&rmi4_data->intr_mask[0],
			sizeof(rmi4_data->intr_mask[0]));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set interrupt enable bit\n",
				__func__);
		return retval;
	}

	return 0;
}

static void synaptics_rmi4_udg_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	if (!udg)
		return;

	if (udg->intr_mask & intr_mask)
		udg_report();

	return;
}

static int synaptics_rmi4_udg_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii;
	unsigned char size;
	unsigned char attr_count;
	unsigned char param_count;

	if (udg) {
		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Handle already exists\n",
				__func__);
		return 0;
	}

	udg = kzalloc(sizeof(*udg), GFP_KERNEL);
	if (!udg) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for udg\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	size = 0;
	for (ii = 0; ii < sizeof(ctrl_18_sub_size); ii++)
		size += ctrl_18_sub_size[ii];
	size += sizeof(struct udg_tuning);
	udg->ctrl_buf = kzalloc(size, GFP_KERNEL);
	if (!udg->ctrl_buf) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for ctrl_buf\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_udg;
	}

	udg->rmi4_data = rmi4_data;

	retval = udg_scan_pdt();
	if (retval < 0)
		goto exit_free_ctrl_buf;

	udg->template_data_buf = kzalloc(udg->template_data_size, GFP_KERNEL);
	if (!udg->template_data_buf) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for template_data_buf\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_ctrl_buf;
	}

#ifdef STORE_GESTURES
	udg->storage_buf = kzalloc(
			udg->template_data_size * udg->gestures_to_store,
			GFP_KERNEL);
	if (!udg->storage_buf) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for storage_buf\n",
				__func__);
		kfree(udg->template_data_buf);
		retval = -ENOMEM;
		goto exit_free_ctrl_buf;
	}
#endif

	udg->udg_dev = input_allocate_device();
	if (udg->udg_dev == NULL) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate gesture device\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_template_data_buf;
	}

	udg->udg_dev->name = GESTURE_DRIVER_NAME;
	udg->udg_dev->phys = GESTURE_PHYS_NAME;
	udg->udg_dev->id.product = SYNAPTICS_DSX_DRIVER_PRODUCT;
	udg->udg_dev->id.version = SYNAPTICS_DSX_DRIVER_VERSION;
	udg->udg_dev->dev.parent = rmi4_data->pdev->dev.parent;
	input_set_drvdata(udg->udg_dev, rmi4_data);

	set_bit(EV_KEY, udg->udg_dev->evbit);
	set_bit(KEY_WAKEUP, udg->udg_dev->keybit);
	input_set_capability(udg->udg_dev, EV_KEY, KEY_WAKEUP);

	retval = input_register_device(udg->udg_dev);
	if (retval) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to register gesture device\n",
				__func__);
		input_free_device(udg->udg_dev);
		goto exit_free_template_data_buf;
	}

	udg->tuning_dir = kobject_create_and_add(TUNING_SYSFS_DIR_NAME,
			&udg->udg_dev->dev.kobj);
	if (!udg->tuning_dir) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to create tuning sysfs directory\n",
				__func__);
		goto exit_unregister_input_device;
	}

	retval = sysfs_create_bin_file(&udg->udg_dev->dev.kobj, &template_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to create template data bin file\n",
				__func__);
		goto exit_remove_sysfs_directory;
	}

	retval = sysfs_create_bin_file(&udg->udg_dev->dev.kobj, &trace_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to create trace data bin file\n",
				__func__);
		goto exit_remove_bin_file;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&udg->udg_dev->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			retval = -ENODEV;
			goto exit_remove_attrs;
		}
	}

	for (param_count = 0; param_count < ARRAY_SIZE(params); param_count++) {
		retval = sysfs_create_file(udg->tuning_dir,
				&params[param_count].attr);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to create tuning parameters\n",
					__func__);
			retval = -ENODEV;
			goto exit_remove_params;
		}
	}

	retval = udg_engine_enable(true);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to enable gesture engine\n",
				__func__);
		goto exit_remove_params;
	}

	return 0;

exit_remove_params:
	for (param_count--; param_count >= 0; param_count--) {
		sysfs_remove_file(udg->tuning_dir,
				&params[param_count].attr);
	}

exit_remove_attrs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&udg->udg_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	sysfs_remove_bin_file(&udg->udg_dev->dev.kobj, &trace_data);

exit_remove_bin_file:
	sysfs_remove_bin_file(&udg->udg_dev->dev.kobj, &template_data);

exit_remove_sysfs_directory:
	kobject_put(udg->tuning_dir);

exit_unregister_input_device:
	input_unregister_device(udg->udg_dev);

exit_free_template_data_buf:
#ifdef STORE_GESTURES
	kfree(udg->storage_buf);
#endif
	kfree(udg->template_data_buf);

exit_free_ctrl_buf:
	kfree(udg->ctrl_buf);

exit_free_udg:
	kfree(udg);
	udg = NULL;

exit:
	return retval;
}

static void synaptics_rmi4_udg_remove(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char count;

	if (!udg)
		goto exit;

	for (count = 0; count < ARRAY_SIZE(params); count++) {
		sysfs_remove_file(udg->tuning_dir,
				&params[count].attr);
	}

	for (count = 0; count < ARRAY_SIZE(attrs); count++) {
		sysfs_remove_file(&udg->udg_dev->dev.kobj,
				&attrs[count].attr);
	}

	sysfs_remove_bin_file(&udg->udg_dev->dev.kobj, &trace_data);
	sysfs_remove_bin_file(&udg->udg_dev->dev.kobj, &template_data);
	kobject_put(udg->tuning_dir);

	input_unregister_device(udg->udg_dev);
#ifdef STORE_GESTURES
	kfree(udg->storage_buf);
#endif
	kfree(udg->template_data_buf);
	kfree(udg->trace_data_buf);
	kfree(udg->ctrl_buf);
	kfree(udg);
	udg = NULL;

exit:
	complete(&udg_remove_complete);

	return;
}

static void synaptics_rmi4_udg_reset(struct synaptics_rmi4_data *rmi4_data)
{
	if (!udg) {
		synaptics_rmi4_udg_init(rmi4_data);
		return;
	}

	udg_scan_pdt();
	udg_engine_enable(true);
#ifdef STORE_GESTURES
	udg_write_template_data();
	udg_write_valid_data();
#endif

	return;
}

static void synaptics_rmi4_udg_reinit(struct synaptics_rmi4_data *rmi4_data)
{
	if (!udg)
		return;

	udg_engine_enable(true);
#ifdef STORE_GESTURES
	udg_write_template_data();
	udg_write_valid_data();
#endif

	return;
}

static void synaptics_rmi4_udg_e_suspend(struct synaptics_rmi4_data *rmi4_data)
{
	if (!udg)
		return;

	rmi4_data->sleep_enable(rmi4_data, false);
	rmi4_data->irq_enable(rmi4_data, true, false);
	enable_irq_wake(rmi4_data->irq);

	udg_engine_enable(true);
	udg_detection_enable(true);

	return;
}

static void synaptics_rmi4_udg_suspend(struct synaptics_rmi4_data *rmi4_data)
{
	if (!udg)
		return;

	rmi4_data->sleep_enable(rmi4_data, false);
	rmi4_data->irq_enable(rmi4_data, true, false);
	enable_irq_wake(rmi4_data->irq);

	udg_engine_enable(true);
	udg_detection_enable(true);

	return;
}

static void synaptics_rmi4_udg_resume(struct synaptics_rmi4_data *rmi4_data)
{
	if (!udg)
		return;

	disable_irq_wake(rmi4_data->irq);
	udg_detection_enable(false);

	return;
}

static void synaptics_rmi4_udg_l_resume(struct synaptics_rmi4_data *rmi4_data)
{
	if (!udg)
		return;

	disable_irq_wake(rmi4_data->irq);
	udg_detection_enable(false);

	return;
}

static struct synaptics_rmi4_exp_fn gesture_module = {
	.fn_type = RMI_GESTURE,
	.init = synaptics_rmi4_udg_init,
	.remove = synaptics_rmi4_udg_remove,
	.reset = synaptics_rmi4_udg_reset,
	.reinit = synaptics_rmi4_udg_reinit,
	.early_suspend = synaptics_rmi4_udg_e_suspend,
	.suspend = synaptics_rmi4_udg_suspend,
	.resume = synaptics_rmi4_udg_resume,
	.late_resume = synaptics_rmi4_udg_l_resume,
	.attn = synaptics_rmi4_udg_attn,
};

static int __init rmi4_gesture_module_init(void)
{
	synaptics_rmi4_new_function_force(&gesture_module, true);

	return 0;
}

static void __exit rmi4_gesture_module_exit(void)
{
	synaptics_rmi4_new_function_force(&gesture_module, false);

	wait_for_completion(&udg_remove_complete);

	return;
}

module_init(rmi4_gesture_module_init);
module_exit(rmi4_gesture_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX User Defined Gesture Module");
MODULE_LICENSE("GPL v2");
