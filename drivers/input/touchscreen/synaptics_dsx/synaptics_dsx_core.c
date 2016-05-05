/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/input/synaptics_dsx_v2.h>
#include "synaptics_dsx_core.h"
#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif
#if defined(CONFIG_SECURE_TOUCH)
#include <linux/errno.h>
#include <soc/qcom/scm.h>
enum subsystem {
	TZ = 1,
	APSS = 3
};

#define TZ_BLSP_MODIFY_OWNERSHIP_ID 3
#endif

#define INPUT_PHYS_NAME "synaptics_dsx/input0"
#define DEBUGFS_DIR_NAME "ts_debug"

#ifdef KERNEL_ABOVE_2_6_38
#define TYPE_B_PROTOCOL
#endif

#define NO_0D_WHILE_2D
#define REPORT_2D_Z
#define REPORT_2D_W

#define F12_DATA_15_WORKAROUND

/*
#define IGNORE_FN_INIT_FAILURE
*/

#define RPT_TYPE (1 << 0)
#define RPT_X_LSB (1 << 1)
#define RPT_X_MSB (1 << 2)
#define RPT_Y_LSB (1 << 3)
#define RPT_Y_MSB (1 << 4)
#define RPT_Z (1 << 5)
#define RPT_WX (1 << 6)
#define RPT_WY (1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

#define EXP_FN_WORK_DELAY_MS 1000 /* ms */
#define MAX_F11_TOUCH_WIDTH 15

#define CHECK_STATUS_TIMEOUT_MS 100

#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18
#define F11_STD_QUERY_LEN 9
#define F11_STD_CTRL_LEN 10
#define F11_STD_DATA_LEN 12

#define STATUS_NO_ERROR 0x00
#define STATUS_RESET_OCCURRED 0x01
#define STATUS_INVALID_CONFIG 0x02
#define STATUS_DEVICE_FAILURE 0x03
#define STATUS_CONFIG_CRC_FAILURE 0x04
#define STATUS_FIRMWARE_CRC_FAILURE 0x05
#define STATUS_CRC_IN_PROGRESS 0x06

#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_OFF (0 << 2)
#define NO_SLEEP_ON (1 << 2)
#define CONFIGURED (1 << 7)

#define SYNA_F11_MAX		4096
#define SYNA_F12_MAX		65536

#define SYNA_S332U_PACKAGE_ID		332
#define SYNA_S332U_PACKAGE_ID_REV		85

static int synaptics_rmi4_f12_set_enables(struct synaptics_rmi4_data *rmi4_data,
		unsigned short ctrl28);

static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data);

static ssize_t synaptics_rmi4_full_pm_cycle_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_full_pm_cycle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#if defined(CONFIG_FB)
static void fb_notify_resume_work(struct work_struct *work);
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void synaptics_rmi4_early_suspend(struct early_suspend *h);

static void synaptics_rmi4_late_resume(struct early_suspend *h);
#endif

static int synaptics_rmi4_suspend(struct device *dev);

static int synaptics_rmi4_resume(struct device *dev);

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_set_abs_x_axis(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_set_abs_y_axis(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static irqreturn_t synaptics_rmi4_irq(int irq, void *data);

#if defined(CONFIG_SECURE_TOUCH)
static ssize_t synaptics_secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf);
#endif

struct synaptics_rmi4_f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
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
		};
		unsigned char data[5];
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
		};
		unsigned char data[3];
	};
};

struct synaptics_rmi4_f12_ctrl_8 {
	union {
		struct {
			unsigned char max_x_coord_lsb;
			unsigned char max_x_coord_msb;
			unsigned char max_y_coord_lsb;
			unsigned char max_y_coord_msb;
			unsigned char rx_pitch_lsb;
			unsigned char rx_pitch_msb;
			unsigned char tx_pitch_lsb;
			unsigned char tx_pitch_msb;
			unsigned char low_rx_clip;
			unsigned char high_rx_clip;
			unsigned char low_tx_clip;
			unsigned char high_tx_clip;
			unsigned char num_of_rx;
			unsigned char num_of_tx;
		};
		unsigned char data[14];
	};
};

struct synaptics_rmi4_f12_ctrl_23 {
	union {
		struct {
			unsigned char obj_type_enable;
			unsigned char max_reported_objects;
		};
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f12_finger_data {
	unsigned char object_type_and_status;
	unsigned char x_lsb;
	unsigned char x_msb;
	unsigned char y_lsb;
	unsigned char y_msb;
#ifdef REPORT_2D_Z
	unsigned char z;
#endif
#ifdef REPORT_2D_W
	unsigned char wx;
	unsigned char wy;
#endif
};

struct synaptics_rmi4_f1a_query {
	union {
		struct {
			unsigned char max_button_count:3;
			unsigned char reserved:5;
			unsigned char has_general_control:1;
			unsigned char has_interrupt_enable:1;
			unsigned char has_multibutton_select:1;
			unsigned char has_tx_rx_map:1;
			unsigned char has_perbutton_threshold:1;
			unsigned char has_release_threshold:1;
			unsigned char has_strongestbtn_hysteresis:1;
			unsigned char has_filter_strength:1;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f1a_control_0 {
	union {
		struct {
			unsigned char multibutton_report:2;
			unsigned char filter_mode:2;
			unsigned char reserved:4;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_control {
	struct synaptics_rmi4_f1a_control_0 general_control;
	unsigned char button_int_enable;
	unsigned char multi_button;
	unsigned char *txrx_map;
	unsigned char *button_threshold;
	unsigned char button_release_threshold;
	unsigned char strongest_button_hysteresis;
	unsigned char filter_strength;
};

struct synaptics_rmi4_f1a_handle {
	int button_bitmask_size;
	unsigned char max_count;
	unsigned char valid_button_count;
	unsigned char *button_data_buffer;
	unsigned char *button_map;
	struct synaptics_rmi4_f1a_query button_query;
	struct synaptics_rmi4_f1a_control button_control;
};

struct synaptics_rmi4_exp_fhandler {
	struct synaptics_rmi4_exp_fn *exp_fn;
	bool insert;
	bool remove;
	struct list_head link;
};

struct synaptics_rmi4_exp_fn_data {
	bool initialized;
	bool queue_work;
	struct mutex mutex;
	struct list_head list;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct synaptics_rmi4_data *rmi4_data;
};

static struct synaptics_rmi4_exp_fn_data exp_data;

static struct device_attribute attrs[] = {
	__ATTR(full_pm_cycle, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_full_pm_cycle_show,
			synaptics_rmi4_full_pm_cycle_store),
	__ATTR(reset, (S_IWUSR | S_IWGRP),
			NULL,
			synaptics_rmi4_f01_reset_store),
	__ATTR(set_abs_x_axis, (S_IWUSR | S_IWGRP),
			NULL,
			synaptics_rmi4_set_abs_x_axis),
	__ATTR(set_abs_y_axis, (S_IWUSR | S_IWGRP),
			NULL,
			synaptics_rmi4_set_abs_y_axis),
	__ATTR(productinfo, S_IRUGO,
			synaptics_rmi4_f01_productinfo_show,
			synaptics_rmi4_store_error),
	__ATTR(buildid, S_IRUGO,
			synaptics_rmi4_f01_buildid_show,
			synaptics_rmi4_store_error),
	__ATTR(flashprog, S_IRUGO,
			synaptics_rmi4_f01_flashprog_show,
			synaptics_rmi4_store_error),
	__ATTR(0dbutton, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_0dbutton_show,
			synaptics_rmi4_0dbutton_store),
#if defined(CONFIG_SECURE_TOUCH)
	__ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_secure_touch_enable_show,
			synaptics_secure_touch_enable_store),
	__ATTR(secure_touch, S_IRUGO ,
			synaptics_secure_touch_show,
			NULL),
#endif
};

#define MAX_BUF_SIZE	256
#define VKEY_VER_CODE	"0x01"

#define HEIGHT_SCALE_NUM 8
#define HEIGHT_SCALE_DENOM 10

/* numerator and denomenator for border equations */
#define BORDER_ADJUST_NUM 3
#define BORDER_ADJUST_DENOM 4

static struct kobject *vkey_kobj;
static char *vkey_buf;

static ssize_t vkey_show(struct kobject  *obj,
	struct kobj_attribute *attr, char *buf)
{
	strlcpy(buf, vkey_buf, MAX_BUF_SIZE);
	return strnlen(buf, MAX_BUF_SIZE);
}

static struct kobj_attribute vkey_obj_attr = {
	.attr = {
		.mode = S_IRUGO,
		.name = "virtualkeys."PLATFORM_DRIVER_NAME,
	},
	.show = vkey_show,
};

static struct attribute *vkey_attr[] = {
	&vkey_obj_attr.attr,
	NULL,
};

static struct attribute_group vkey_grp = {
	.attrs = vkey_attr,
};

static int synaptics_rmi4_debug_suspend_set(void *_data, u64 val)
{
	struct synaptics_rmi4_data *rmi4_data = _data;

	if (val)
		synaptics_rmi4_suspend(&rmi4_data->input_dev->dev);
	else
		synaptics_rmi4_resume(&rmi4_data->input_dev->dev);

	return 0;
}

static int synaptics_rmi4_debug_suspend_get(void *_data, u64 *val)
{
	struct synaptics_rmi4_data *rmi4_data = _data;

	*val = rmi4_data->suspended;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_suspend_fops, synaptics_rmi4_debug_suspend_get,
			synaptics_rmi4_debug_suspend_set, "%lld\n");

#if defined(CONFIG_SECURE_TOUCH)
static int synaptics_i2c_change_pipe_owner(
	struct synaptics_rmi4_data *rmi4_data, enum subsystem subsystem)
{
	/*scm call descriptor */
	struct scm_desc desc;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	int ret = 0;

	/* number of arguments */
	desc.arginfo = SCM_ARGS(2);
	/* BLSPID (1-12) */
	desc.args[0] = i2c->adapter->nr - 1;
	 /* Owner if TZ or APSS */
	desc.args[1] = subsystem;
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_TZ, TZ_BLSP_MODIFY_OWNERSHIP_ID),
			&desc);
	if (ret)
		return ret;

	return desc.ret[0];
}

static void synaptics_secure_touch_init(struct synaptics_rmi4_data *data)
{
	int ret = 0;
	data->st_initialized = 0;
	init_completion(&data->st_powerdown);
	init_completion(&data->st_irq_processed);
	/* Get clocks */
	data->core_clk = clk_get(data->pdev->dev.parent, "core_clk");
	if (IS_ERR(data->core_clk)) {
		ret = PTR_ERR(data->core_clk);
		dev_err(data->pdev->dev.parent,
			"%s: error on clk_get(core_clk):%d\n", __func__, ret);
		return;
	}

	data->iface_clk = clk_get(data->pdev->dev.parent, "iface_clk");
	if (IS_ERR(data->iface_clk)) {
		ret = PTR_ERR(data->iface_clk);
		dev_err(data->pdev->dev.parent,
			"%s: error on clk_get(iface_clk):%d\n", __func__, ret);
		goto err_iface_clk;
	}

	data->st_initialized = 1;
	return;

err_iface_clk:
		clk_put(data->core_clk);
		data->core_clk = NULL;
}
static void synaptics_secure_touch_notify(struct synaptics_rmi4_data *rmi4_data)
{
	sysfs_notify(&rmi4_data->input_dev->dev.kobj, NULL, "secure_touch");
}
static irqreturn_t synaptics_filter_interrupt(
	struct synaptics_rmi4_data *rmi4_data)
{
	if (atomic_read(&rmi4_data->st_enabled)) {
		if (atomic_cmpxchg(&rmi4_data->st_pending_irqs, 0, 1) == 0) {
			reinit_completion(&rmi4_data->st_irq_processed);
			synaptics_secure_touch_notify(rmi4_data);
			wait_for_completion_interruptible(
				&rmi4_data->st_irq_processed);
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}
static void synaptics_secure_touch_stop(
	struct synaptics_rmi4_data *rmi4_data,
	int blocking)
{
	if (atomic_read(&rmi4_data->st_enabled)) {
		atomic_set(&rmi4_data->st_pending_irqs, -1);
		synaptics_secure_touch_notify(rmi4_data);
		if (blocking)
			wait_for_completion_interruptible(
				&rmi4_data->st_powerdown);
	}
}
#else
static void synaptics_secure_touch_init(struct synaptics_rmi4_data *rmi4_data)
{
}
static irqreturn_t synaptics_filter_interrupt(
	struct synaptics_rmi4_data *rmi4_data)
{
	return IRQ_NONE;
}
static void synaptics_secure_touch_stop(
	struct synaptics_rmi4_data *rmi4_data,
	int blocking)
{
}
#endif

#if defined(CONFIG_SECURE_TOUCH)
static ssize_t synaptics_secure_touch_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	return scnprintf(
		buf,
		PAGE_SIZE,
		"%d",
		atomic_read(&rmi4_data->st_enabled));
}
/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process and
 * the interrupt handler.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t synaptics_secure_touch_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	unsigned long value;
	int err = 0;

	if (count > 2)
		return -EINVAL;

	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	if (!rmi4_data->st_initialized)
		return -EIO;

	err = count;

	switch (value) {
	case 0:
		if (atomic_read(&rmi4_data->st_enabled) == 0)
			break;

		synaptics_i2c_change_pipe_owner(rmi4_data, APSS);
		synaptics_rmi4_bus_put(rmi4_data);
		atomic_set(&rmi4_data->st_enabled, 0);
		synaptics_secure_touch_notify(rmi4_data);
		complete(&rmi4_data->st_irq_processed);
		synaptics_rmi4_irq(rmi4_data->irq, rmi4_data);
		complete(&rmi4_data->st_powerdown);

		break;
	case 1:
		if (atomic_read(&rmi4_data->st_enabled)) {
			err = -EBUSY;
			break;
		}

		synchronize_irq(rmi4_data->irq);

		if (synaptics_rmi4_bus_get(rmi4_data) < 0) {
			dev_err(
				rmi4_data->pdev->dev.parent,
				"synaptics_rmi4_bus_get failed\n");
			err = -EIO;
			break;
		}
		synaptics_i2c_change_pipe_owner(rmi4_data, TZ);
		reinit_completion(&rmi4_data->st_powerdown);
		reinit_completion(&rmi4_data->st_irq_processed);
		atomic_set(&rmi4_data->st_enabled, 1);
		atomic_set(&rmi4_data->st_pending_irqs,  0);
		break;
	default:
		dev_err(
			rmi4_data->pdev->dev.parent,
			"unsupported value: %lu\n", value);
		err = -EINVAL;
		break;
	}
	return err;
}

/*
 * This function returns whether there are pending interrupts, or
 * other error conditions that need to be signaled to the userspace library,
 * according tot he following logic:
 * - st_enabled is 0 if secure touch is not enabled, returning -EBADF
 * - st_pending_irqs is -1 to signal that secure touch is in being stopped,
 *   returning -EINVAL
 * - st_pending_irqs is 1 to signal that there is a pending irq, returning
 *   the value "1" to the sysfs read operation
 * - st_pending_irqs is 0 (only remaining case left) if the pending interrupt
 *   has been processed, so the interrupt handler can be allowed to continue.
 */
static ssize_t synaptics_secure_touch_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	int val = 0;
	if (atomic_read(&rmi4_data->st_enabled) == 0)
		return -EBADF;

	if (atomic_cmpxchg(&rmi4_data->st_pending_irqs, -1, 0) == -1)
		return -EINVAL;

	if (atomic_cmpxchg(&rmi4_data->st_pending_irqs, 1, 0) == 1)
		val = 1;
	else
		complete(&rmi4_data->st_irq_processed);

	return scnprintf(buf, PAGE_SIZE, "%u", val);

}
#endif

static ssize_t synaptics_rmi4_full_pm_cycle_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->full_pm_cycle);
}

static ssize_t synaptics_rmi4_full_pm_cycle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmi4_data->full_pm_cycle = input > 0 ? 1 : 0;

	return count;
}

static ssize_t synaptics_rmi4_set_abs_x_axis(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 0)
		return -EINVAL;

	input_set_abs_params(rmi4_data->input_dev, ABS_MT_POSITION_X,
			0, input, 0, 0);

	return count;
}

static ssize_t synaptics_rmi4_set_abs_y_axis(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 0)
		return -EINVAL;

	input_set_abs_params(rmi4_data->input_dev, ABS_MT_POSITION_Y,
			0, input, 0, 0);

	return count;
}

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int reset;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	retval = synaptics_rmi4_reset_device(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x 0x%02x\n",
			(rmi4_data->rmi4_mod_info.product_info[0]),
			(rmi4_data->rmi4_mod_info.product_info[1]));
}

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->firmware_id);
}

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct synaptics_rmi4_f01_device_status device_status;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			device_status.data,
			sizeof(device_status.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read device status, error = %d\n",
				__func__, retval);
		return retval;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
			device_status.flash_prog);
}

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->button_0d_enabled);
}

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	unsigned char ii;
	unsigned char intr_enable;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	input = input > 0 ? 1 : 0;

	if (rmi4_data->button_0d_enabled == input)
		return count;

	if (list_empty(&rmi->support_fn_list))
		return -ENODEV;

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F1A) {
			ii = fhandler->intr_reg_num;

			retval = synaptics_rmi4_reg_read(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return retval;

			if (input == 1)
				intr_enable |= fhandler->intr_mask;
			else
				intr_enable &= ~fhandler->intr_mask;

			retval = synaptics_rmi4_reg_write(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return retval;
		}
	}

	rmi4_data->button_0d_enabled = input;

	return count;
}

 /**
 * synaptics_rmi4_f11_abs_report()
 *
 * Called by synaptics_rmi4_report_touch() when valid Function $11
 * finger data has been detected.
 *
 * This function reads the Function $11 data registers, determines the
 * status of each finger supported by the Function, processes any
 * necessary coordinate manipulation, reports the finger data to
 * the input subsystem, and returns the number of fingers detected.
 */
static int synaptics_rmi4_f11_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char reg_index;
	unsigned char finger;
	unsigned char fingers_supported;
	unsigned char num_of_finger_status_regs;
	unsigned char finger_shift;
	unsigned char finger_status;
	unsigned char data_reg_blk_size;
	unsigned char finger_status_reg[3];
	unsigned char data[F11_STD_DATA_LEN];
	unsigned short data_addr;
	unsigned short data_offset;
	int x;
	int y;
	int wx;
	int wy;
	int temp;

	/*
	 * The number of finger status registers is determined by the
	 * maximum number of fingers supported - 2 bits per finger. So
	 * the number of finger status registers to read is:
	 * register_count = ceil(max_num_of_fingers / 4)
	 */
	fingers_supported = fhandler->num_of_data_points;
	num_of_finger_status_regs = (fingers_supported + 3) / 4;
	data_addr = fhandler->full_addr.data_base;
	data_reg_blk_size = fhandler->size_of_data_register_block;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr,
			finger_status_reg,
			num_of_finger_status_regs);
	if (retval < 0)
		return 0;

	for (finger = 0; finger < fingers_supported; finger++) {
		reg_index = finger / 4;
		finger_shift = (finger % 4) * 2;
		finger_status = (finger_status_reg[reg_index] >> finger_shift)
				& MASK_2BIT;

		/*
		 * Each 2-bit finger status field represents the following:
		 * 00 = finger not present
		 * 01 = finger present and data accurate
		 * 10 = finger present but data may be inaccurate
		 * 11 = reserved
		 */
#ifdef TYPE_B_PROTOCOL
		input_mt_slot(rmi4_data->input_dev, finger);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, finger_status);
#endif

		if (finger_status) {
			data_offset = data_addr +
					num_of_finger_status_regs +
					(finger * data_reg_blk_size);
			retval = synaptics_rmi4_reg_read(rmi4_data,
					data_offset,
					data,
					data_reg_blk_size);
			if (retval < 0)
				return 0;

			x = (data[0] << 4) | (data[2] & MASK_4BIT);
			y = (data[1] << 4) | ((data[2] >> 4) & MASK_4BIT);
			wx = (data[3] & MASK_4BIT);
			wy = (data[3] >> 4) & MASK_4BIT;

			if (rmi4_data->hw_if->board_data->swap_axes) {
				temp = x;
				x = y;
				y = temp;
				temp = wx;
				wx = wy;
				wy = temp;
			}

			if (rmi4_data->hw_if->board_data->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->hw_if->board_data->y_flip)
				y = rmi4_data->sensor_max_y - y;

			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 1);
			input_report_key(rmi4_data->input_dev,
					BTN_TOOL_FINGER, 1);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(rmi4_data->input_dev);
#endif

			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: Finger %d:\n"
					"status = 0x%02x\n"
					"x = %d\n"
					"y = %d\n"
					"wx = %d\n"
					"wy = %d\n",
					__func__, finger,
					finger_status,
					x, y, wx, wy);

			touch_count++;
		}
	}

	if (touch_count == 0) {
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
		input_mt_sync(rmi4_data->input_dev);
#endif
	}

	input_sync(rmi4_data->input_dev);

	return touch_count;
}

 /**
 * synaptics_rmi4_f12_abs_report()
 *
 * Called by synaptics_rmi4_report_touch() when valid Function $12
 * finger data has been detected.
 *
 * This function reads the Function $12 data registers, determines the
 * status of each finger supported by the Function, processes any
 * necessary coordinate manipulation, reports the finger data to
 * the input subsystem, and returns the number of fingers detected.
 */
static int synaptics_rmi4_f12_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char finger;
	unsigned char fingers_to_process;
	unsigned char finger_status;
	unsigned char size_of_2d_data;
	unsigned short data_addr;
	int x;
	int y;
	int wx;
	int wy;
	int temp;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_finger_data *data;
	struct synaptics_rmi4_f12_finger_data *finger_data;
#ifdef F12_DATA_15_WORKAROUND
	static unsigned char fingers_already_present;
#endif

	fingers_to_process = fhandler->num_of_data_points;
	data_addr = fhandler->full_addr.data_base;
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);


	/* Determine the total number of fingers to process */
	if (extra_data->data15_size) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data15_offset,
				extra_data->data15_data,
				extra_data->data15_size);
		if (retval < 0)
			return 0;

		/* Start checking from the highest bit */
		temp = extra_data->data15_size - 1; /* Highest byte */
		if ((temp >= 0) && (fingers_to_process > 0) &&
		    (temp < ((F12_FINGERS_TO_SUPPORT + 7) / 8))) {
			finger = (fingers_to_process - 1) % 8; /* Highest bit */
			do {
				if (extra_data->data15_data[temp]
					& (1 << finger))
					break;

				if (finger) {
					finger--;
				} else {
					/* Move to the next lower byte */
					temp--;
					finger = 7;
				}

				fingers_to_process--;
			} while (fingers_to_process && (temp >= 0));
		}
		dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Number of fingers to process = %d\n",
			__func__, fingers_to_process);
	}

#ifdef F12_DATA_15_WORKAROUND
	fingers_to_process = max(fingers_to_process, fingers_already_present);
#endif

	if (!fingers_to_process) {
		synaptics_rmi4_free_fingers(rmi4_data);
		return 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr + extra_data->data1_offset,
			(unsigned char *)fhandler->data,
			fingers_to_process * size_of_2d_data);
	if (retval < 0)
		return 0;

	data = (struct synaptics_rmi4_f12_finger_data *)fhandler->data;

	for (finger = 0; finger < fingers_to_process; finger++) {
		finger_data = data + finger;
		finger_status = finger_data->object_type_and_status;

		if (finger_status == F12_FINGER_STATUS) {
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(rmi4_data->input_dev, finger);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 1);
#endif

#ifdef F12_DATA_15_WORKAROUND
			fingers_already_present = finger + 1;
#endif

			x = (finger_data->x_msb << 8) | (finger_data->x_lsb);
			y = (finger_data->y_msb << 8) | (finger_data->y_lsb);
#ifdef REPORT_2D_W
			wx = finger_data->wx;
			wy = finger_data->wy;
#endif

			if (rmi4_data->hw_if->board_data->swap_axes) {
				temp = x;
				x = y;
				y = temp;
				temp = wx;
				wx = wy;
				wy = temp;
			}

			if (rmi4_data->hw_if->board_data->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->hw_if->board_data->y_flip)
				y = rmi4_data->sensor_max_y - y;

			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 1);
			input_report_key(rmi4_data->input_dev,
					BTN_TOOL_FINGER, 1);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(rmi4_data->input_dev);
#endif

			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: Finger %d:\n"
					"status = 0x%02x\n"
					"x = %d\n"
					"y = %d\n"
					"wx = %d\n"
					"wy = %d\n",
					__func__, finger,
					finger_status,
					x, y, wx, wy);

			touch_count++;
		} else {
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(rmi4_data->input_dev, finger);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 0);
#endif
		}
	}

	if (touch_count == 0) {
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
		input_mt_sync(rmi4_data->input_dev);
#endif
	}

	input_sync(rmi4_data->input_dev);

	return touch_count;
}

static void synaptics_rmi4_f1a_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0;
	unsigned char button;
	unsigned char index;
	unsigned char shift;
	unsigned char status;
	unsigned char *data;
	unsigned short data_addr = fhandler->full_addr.data_base;
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	static unsigned char do_once = 1;
	static bool current_status[MAX_NUMBER_OF_BUTTONS];
#ifdef NO_0D_WHILE_2D
	static bool before_2d_status[MAX_NUMBER_OF_BUTTONS];
	static bool while_2d_status[MAX_NUMBER_OF_BUTTONS];
#endif

	if (do_once) {
		memset(current_status, 0, sizeof(current_status));
#ifdef NO_0D_WHILE_2D
		memset(before_2d_status, 0, sizeof(before_2d_status));
		memset(while_2d_status, 0, sizeof(while_2d_status));
#endif
		do_once = 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr,
			f1a->button_data_buffer,
			f1a->button_bitmask_size);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read button data registers\n",
				__func__);
		return;
	}

	data = f1a->button_data_buffer;

	for (button = 0; button < f1a->valid_button_count; button++) {
		index = button / 8;
		shift = button % 8;
		status = ((data[index] >> shift) & MASK_1BIT);

		if (current_status[button] == status)
			continue;
		else
			current_status[button] = status;

		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Button %d (code %d) ->%d\n",
				__func__, button,
				f1a->button_map[button],
				status);
#ifdef NO_0D_WHILE_2D
		if (rmi4_data->fingers_on_2d == false) {
			if (status == 1) {
				before_2d_status[button] = 1;
			} else {
				if (while_2d_status[button] == 1) {
					while_2d_status[button] = 0;
					continue;
				} else {
					before_2d_status[button] = 0;
				}
			}
			touch_count++;
			input_report_key(rmi4_data->input_dev,
					f1a->button_map[button],
					status);
		} else {
			if (before_2d_status[button] == 1) {
				before_2d_status[button] = 0;
				touch_count++;
				input_report_key(rmi4_data->input_dev,
						f1a->button_map[button],
						status);
			} else {
				if (status == 1)
					while_2d_status[button] = 1;
				else
					while_2d_status[button] = 0;
			}
		}
#else
		touch_count++;
		input_report_key(rmi4_data->input_dev,
				f1a->button_map[button],
				status);
#endif
	}

	if (touch_count)
		input_sync(rmi4_data->input_dev);

	return;
}

 /**
 * synaptics_rmi4_report_touch()
 *
 * Called by synaptics_rmi4_sensor_report().
 *
 * This function calls the appropriate finger data reporting function
 * based on the function handler it receives and returns the number of
 * fingers detected.
 */
static void synaptics_rmi4_report_touch(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	unsigned char touch_count_2d;

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Function %02x reporting\n",
			__func__, fhandler->fn_number);

	switch (fhandler->fn_number) {
	case SYNAPTICS_RMI4_F11:
		touch_count_2d = synaptics_rmi4_f11_abs_report(rmi4_data,
				fhandler);

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;
	case SYNAPTICS_RMI4_F12:
		touch_count_2d = synaptics_rmi4_f12_abs_report(rmi4_data,
				fhandler);

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;
	case SYNAPTICS_RMI4_F1A:
		synaptics_rmi4_f1a_report(rmi4_data, fhandler);
		break;
	default:
		break;
	}

	return;
}

 /**
 * synaptics_rmi4_sensor_report()
 *
 * Called by synaptics_rmi4_irq().
 *
 * This function determines the interrupt source(s) from the sensor
 * and calls synaptics_rmi4_report_touch() with the appropriate
 * function handler for each function with valid data inputs.
 */
static void synaptics_rmi4_sensor_report(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char data[MAX_INTR_REGISTERS + 1];
	unsigned char *intr = &data[1];
	struct synaptics_rmi4_f01_device_status status;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	/*
	 * Get interrupt status information from F01 Data1 register to
	 * determine the source(s) that are flagging the interrupt.
	 */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			data,
			rmi4_data->num_of_intr_regs + 1);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read interrupt status\n",
				__func__);
		return;
	}

	status.data[0] = data[0];
	if (status.unconfigured && !status.flash_prog) {
		pr_notice("%s: spontaneous reset detected\n", __func__);
		retval = synaptics_rmi4_reinit_device(rmi4_data);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to reinit device\n",
					__func__);
		}
		return;
	}

	/*
	 * Traverse the function handler list and service the source(s)
	 * of the interrupt accordingly.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				if (fhandler->intr_mask &
						intr[fhandler->intr_reg_num]) {
					synaptics_rmi4_report_touch(rmi4_data,
							fhandler);
				}
			}
		}
	}

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link) {
			if (!exp_fhandler->insert &&
					!exp_fhandler->remove &&
					(exp_fhandler->exp_fn->attn != NULL))
				exp_fhandler->exp_fn->attn(rmi4_data, intr[0]);
		}
	}
	mutex_unlock(&exp_data.mutex);

	return;
}

 /**
 * synaptics_rmi4_irq()
 *
 * Called by the kernel when an interrupt occurs (when the sensor
 * asserts the attention irq).
 *
 * This function is the ISR thread and handles the acquisition
 * and the reporting of finger data when the presence of fingers
 * is detected.
 */
static irqreturn_t synaptics_rmi4_irq(int irq, void *data)
{
	struct synaptics_rmi4_data *rmi4_data = data;

	if (IRQ_HANDLED == synaptics_filter_interrupt(data))
		return IRQ_HANDLED;

	synaptics_rmi4_sensor_report(rmi4_data);

	return IRQ_HANDLED;
}

 /**
 * synaptics_rmi4_irq_enable()
 *
 * Called by synaptics_rmi4_probe() and the power management functions
 * in this driver and also exported to other expansion Function modules
 * such as rmi_dev.
 *
 * This function handles the enabling and disabling of the attention
 * irq including the setting up of the ISR thread.
 */
static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval = 0;
	unsigned char intr_status[MAX_INTR_REGISTERS];
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (enable) {
		if (rmi4_data->irq_enabled)
			return retval;

		/* Clear interrupts first */
		retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01_data_base_addr + 1,
				intr_status,
				rmi4_data->num_of_intr_regs);
		if (retval < 0)
			return retval;

		retval = request_threaded_irq(rmi4_data->irq, NULL,
				synaptics_rmi4_irq, bdata->irq_flags,
				PLATFORM_DRIVER_NAME, rmi4_data);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to create irq thread\n",
					__func__);
			return retval;
		}

		rmi4_data->irq_enabled = true;
	} else {
		if (rmi4_data->irq_enabled) {
			disable_irq(rmi4_data->irq);
			free_irq(rmi4_data->irq, rmi4_data);
			rmi4_data->irq_enabled = false;
		}
	}

	return retval;
}

static void synaptics_rmi4_set_intr_mask(struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	unsigned char ii;
	unsigned char intr_offset;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < ((fd->intr_src_count & MASK_3BIT) +
			intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	return;
}

static int synaptics_rmi4_f01_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	fhandler->data = NULL;
	fhandler->extra = NULL;

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	rmi4_data->f01_query_base_addr = fd->query_base_addr;
	rmi4_data->f01_ctrl_base_addr = fd->ctrl_base_addr;
	rmi4_data->f01_data_base_addr = fd->data_base_addr;
	rmi4_data->f01_cmd_base_addr = fd->cmd_base_addr;

	return 0;
}

 /**
  * synaptics_rmi4_f11_set_coords()
  *
  * Set panel resolution for f11 to match display resolution.
  *
  */
static int synaptics_rmi4_f11_set_coords(struct synaptics_rmi4_data *rmi4_data,
			struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char control[F11_STD_CTRL_LEN];
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (!rmi4_data->update_coords) {
		dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: No need to update panel resolution\n", __func__);
		return 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base,
			control,
			sizeof(control));
	if (retval < 0)
		return retval;

	/* Maximum x and y */
	rmi4_data->sensor_max_x = ((control[6] & MASK_8BIT) << 0) |
			((control[7] & MASK_4BIT) << 8);
	rmi4_data->sensor_max_y = ((control[8] & MASK_8BIT) << 0) |
			((control[9] & MASK_4BIT) << 8);

	if (bdata->panel_maxx && bdata->panel_maxy &&
		(rmi4_data->sensor_max_x != bdata->panel_maxx ||
			rmi4_data->sensor_max_y != bdata->panel_maxy)) {
		if (bdata->panel_maxx > SYNA_F11_MAX ||
				bdata->panel_maxy > SYNA_F11_MAX) {
			dev_err(rmi4_data->pdev->dev.parent,
				"%s: Invalid panel resolution\n", __func__);
			return -EINVAL;
		}

		rmi4_data->sensor_max_x = bdata->panel_maxx;
		rmi4_data->sensor_max_y = bdata->panel_maxy;
		control[6] = rmi4_data->sensor_max_x & MASK_8BIT;
		control[7] = (rmi4_data->sensor_max_x >> 8) & MASK_4BIT;
		control[8] = rmi4_data->sensor_max_y & MASK_8BIT;
		control[9] = (rmi4_data->sensor_max_y >> 8) & MASK_4BIT;

		retval = synaptics_rmi4_reg_write(rmi4_data,
				fhandler->full_addr.ctrl_base,
				control,
				sizeof(control));
		if (retval < 0)
			return retval;
		rmi4_data->update_coords = true;
	} else {
		rmi4_data->update_coords = false;
	}

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	rmi4_data->max_touch_width = MAX_F11_TOUCH_WIDTH;

	return 0;
}

 /**
  * synaptics_rmi4_f11_init()
  *
  * Called by synaptics_rmi4_query_device().
  *
  * This funtion parses information from the Function 11 registers
  * and determines the number of fingers supported, x and y data ranges,
  * offset to the associated interrupt status register, interrupt bit
  * mask, and gathers finger data acquisition capabilities from the query
  * registers.
  */
static int synaptics_rmi4_f11_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char abs_data_size;
	unsigned char abs_data_blk_size;
	unsigned char query[F11_STD_QUERY_LEN];

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base,
			query,
			sizeof(query));
	if (retval < 0)
		return retval;

	/* Maximum number of fingers supported */
	if ((query[1] & MASK_3BIT) <= 4)
		fhandler->num_of_data_points = (query[1] & MASK_3BIT) + 1;
	else if ((query[1] & MASK_3BIT) == 5)
		fhandler->num_of_data_points = 10;

	rmi4_data->num_of_fingers = fhandler->num_of_data_points;

	retval = synaptics_rmi4_f11_set_coords(rmi4_data, fhandler);
	if (retval < 0)
		return retval;

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	abs_data_size = query[5] & MASK_2BIT;
	abs_data_blk_size = 3 + (2 * (abs_data_size == 0 ? 1 : 0));
	fhandler->size_of_data_register_block = abs_data_blk_size;
	fhandler->data = NULL;
	fhandler->extra = NULL;

	return retval;
}

static int synaptics_rmi4_f12_set_enables(struct synaptics_rmi4_data *rmi4_data,
		unsigned short ctrl28)
{
	int retval;
	static unsigned short ctrl_28_address;

	if (ctrl28)
		ctrl_28_address = ctrl28;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			ctrl_28_address,
			&rmi4_data->report_enable,
			sizeof(rmi4_data->report_enable));
	if (retval < 0)
		return retval;

	return retval;
}

 /**
  * synaptics_rmi4_f12_set_coords()
  *
  * Set panel resolution for f12 to match display resolution.
  *
  */
static int synaptics_rmi4_f12_set_coords(struct synaptics_rmi4_data *rmi4_data,
			struct synaptics_rmi4_fn *fhandler)
{

	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;
	struct synaptics_rmi4_f12_query_5 query_5;
	struct synaptics_rmi4_f12_ctrl_8 ctrl_8;
	unsigned char ctrl_8_offset;
	int retval;

	if (!rmi4_data->update_coords) {
		dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: No need to update panel resolution\n", __func__);
		return 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 5,
			query_5.data,
			sizeof(query_5.data));
	if (retval < 0)
		return retval;

	ctrl_8_offset = query_5.ctrl0_is_present +
			query_5.ctrl1_is_present +
			query_5.ctrl2_is_present +
			query_5.ctrl3_is_present +
			query_5.ctrl4_is_present +
			query_5.ctrl5_is_present +
			query_5.ctrl6_is_present +
			query_5.ctrl7_is_present;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_8_offset,
			ctrl_8.data,
			sizeof(ctrl_8.data));
	if (retval < 0)
		return retval;

	/* Maximum x and y */
	rmi4_data->sensor_max_x =
			((unsigned short)ctrl_8.max_x_coord_lsb << 0) |
			((unsigned short)ctrl_8.max_x_coord_msb << 8);
	rmi4_data->sensor_max_y =
			((unsigned short)ctrl_8.max_y_coord_lsb << 0) |
			((unsigned short)ctrl_8.max_y_coord_msb << 8);

	if (bdata->panel_maxx && bdata->panel_maxy &&
		(rmi4_data->sensor_max_x != bdata->panel_maxx ||
			rmi4_data->sensor_max_y != bdata->panel_maxy)) {

		if (bdata->panel_maxx > SYNA_F12_MAX ||
				bdata->panel_maxy > SYNA_F12_MAX) {
			dev_err(rmi4_data->pdev->dev.parent,
				"%s: Invalid panel resolution\n", __func__);
			retval = -EINVAL;
			return retval;
		}

		rmi4_data->sensor_max_x = bdata->panel_maxx;
		rmi4_data->sensor_max_y = bdata->panel_maxy;
		ctrl_8.max_x_coord_lsb = rmi4_data->sensor_max_x & MASK_8BIT;
		ctrl_8.max_x_coord_msb = (rmi4_data->sensor_max_x >> 8) &
								MASK_4BIT;
		ctrl_8.max_y_coord_lsb = rmi4_data->sensor_max_y & MASK_8BIT;
		ctrl_8.max_y_coord_msb = (rmi4_data->sensor_max_y >> 8) &
								MASK_4BIT;

		retval = synaptics_rmi4_reg_write(rmi4_data,
				fhandler->full_addr.ctrl_base + ctrl_8_offset,
				ctrl_8.data,
				sizeof(ctrl_8.data));
		if (retval < 0)
			return retval;
		rmi4_data->update_coords = true;
	} else {
		rmi4_data->update_coords = false;
	}

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	rmi4_data->num_of_rx = ctrl_8.num_of_rx;
	rmi4_data->num_of_tx = ctrl_8.num_of_tx;
	rmi4_data->max_touch_width = max(rmi4_data->num_of_rx,
			rmi4_data->num_of_tx);

	return 0;
}

 /**
  * synaptics_rmi4_f12_init()
  *
  * Called by synaptics_rmi4_query_device().
  *
  * This funtion parses information from the Function 12 registers and
  * determines the number of fingers supported, offset to the data1
  * register, x and y data ranges, offset to the associated interrupt
  * status register, interrupt bit mask, and allocates memory resources
  * for finger data acquisition.
  */
static int synaptics_rmi4_f12_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char size_of_2d_data;
	unsigned char size_of_query8;
	unsigned char ctrl_8_offset;
	unsigned char ctrl_23_offset;
	unsigned char ctrl_28_offset;
	unsigned char num_of_fingers;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_query_5 query_5;
	struct synaptics_rmi4_f12_query_8 query_8;
	struct synaptics_rmi4_f12_ctrl_23 ctrl_23;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	fhandler->extra = kmalloc(sizeof(*extra_data), GFP_KERNEL);
	if (!fhandler->extra) {
		dev_err(rmi4_data->pdev->dev.parent,
			"%s: Failed to alloc mem for function handler\n",
			__func__);
		return -ENOMEM;
	}
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 5,
			query_5.data,
			sizeof(query_5.data));
	if (retval < 0)
		goto free_function_handler_mem;

	ctrl_8_offset = query_5.ctrl0_is_present +
			query_5.ctrl1_is_present +
			query_5.ctrl2_is_present +
			query_5.ctrl3_is_present +
			query_5.ctrl4_is_present +
			query_5.ctrl5_is_present +
			query_5.ctrl6_is_present +
			query_5.ctrl7_is_present;

	ctrl_23_offset = ctrl_8_offset +
			query_5.ctrl8_is_present +
			query_5.ctrl9_is_present +
			query_5.ctrl10_is_present +
			query_5.ctrl11_is_present +
			query_5.ctrl12_is_present +
			query_5.ctrl13_is_present +
			query_5.ctrl14_is_present +
			query_5.ctrl15_is_present +
			query_5.ctrl16_is_present +
			query_5.ctrl17_is_present +
			query_5.ctrl18_is_present +
			query_5.ctrl19_is_present +
			query_5.ctrl20_is_present +
			query_5.ctrl21_is_present +
			query_5.ctrl22_is_present;

	ctrl_28_offset = ctrl_23_offset +
			query_5.ctrl23_is_present +
			query_5.ctrl24_is_present +
			query_5.ctrl25_is_present +
			query_5.ctrl26_is_present +
			query_5.ctrl27_is_present;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_23_offset,
			ctrl_23.data,
			sizeof(ctrl_23.data));
	if (retval < 0)
		goto free_function_handler_mem;

	/* Maximum number of fingers supported */
	fhandler->num_of_data_points = min(ctrl_23.max_reported_objects,
			(unsigned char)F12_FINGERS_TO_SUPPORT);

	num_of_fingers = fhandler->num_of_data_points;
	rmi4_data->num_of_fingers = num_of_fingers;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 7,
			&size_of_query8,
			sizeof(size_of_query8));
	if (retval < 0)
		goto free_function_handler_mem;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base + 8,
			query_8.data,
			size_of_query8);
	if (retval < 0)
		goto free_function_handler_mem;

	/* Determine the presence of the Data0 register */
	extra_data->data1_offset = query_8.data0_is_present;

	if ((size_of_query8 >= 3) && (query_8.data15_is_present)) {
		extra_data->data15_offset = query_8.data0_is_present +
				query_8.data1_is_present +
				query_8.data2_is_present +
				query_8.data3_is_present +
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
				query_8.data14_is_present;
		extra_data->data15_size = (num_of_fingers + 7) / 8;
	} else {
		extra_data->data15_size = 0;
	}

	rmi4_data->report_enable = RPT_DEFAULT;
#ifdef REPORT_2D_Z
	rmi4_data->report_enable |= RPT_Z;
#endif
#ifdef REPORT_2D_W
	rmi4_data->report_enable |= (RPT_WX | RPT_WY);
#endif

	retval = synaptics_rmi4_f12_set_enables(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_28_offset);
	if (retval < 0)
		goto free_function_handler_mem;


	retval = synaptics_rmi4_f12_set_coords(rmi4_data, fhandler);
	if (retval < 0)
		goto free_function_handler_mem;

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	/* Allocate memory for finger data storage space */
	fhandler->data_size = num_of_fingers * size_of_2d_data;
	fhandler->data = kmalloc(fhandler->data_size, GFP_KERNEL);
	if (!fhandler->data) {
		dev_err(rmi4_data->pdev->dev.parent,
			"%s: Failed to alloc mem for function handler data\n",
			__func__);
		retval = -ENOMEM;
		goto free_function_handler_mem;
	}

	return retval;

free_function_handler_mem:
	kfree(fhandler->extra);
	return retval;
}

static int synaptics_rmi4_f1a_alloc_mem(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	struct synaptics_rmi4_f1a_handle *f1a;

	f1a = kzalloc(sizeof(*f1a), GFP_KERNEL);
	if (!f1a) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for function handle\n",
				__func__);
		return -ENOMEM;
	}

	fhandler->data = (void *)f1a;
	fhandler->extra = NULL;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.query_base,
			f1a->button_query.data,
			sizeof(f1a->button_query.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read query registers\n",
				__func__);
		return retval;
	}

	f1a->max_count = f1a->button_query.max_button_count + 1;

	f1a->button_control.txrx_map = kzalloc(f1a->max_count * 2, GFP_KERNEL);
	if (!f1a->button_control.txrx_map) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for tx rx mapping\n",
				__func__);
		return -ENOMEM;
	}

	f1a->button_bitmask_size = (f1a->max_count + 7) / 8;

	f1a->button_data_buffer = kcalloc(f1a->button_bitmask_size,
			sizeof(*(f1a->button_data_buffer)), GFP_KERNEL);
	if (!f1a->button_data_buffer) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for data buffer\n",
				__func__);
		return -ENOMEM;
	}

	f1a->button_map = kcalloc(f1a->max_count,
			sizeof(*(f1a->button_map)), GFP_KERNEL);
	if (!f1a->button_map) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for button map\n",
				__func__);
		return -ENOMEM;
	}

	return 0;
}

static int synaptics_rmi4_f1a_button_map(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char ii;
	unsigned char mapping_offset = 0;
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	mapping_offset = f1a->button_query.has_general_control +
			f1a->button_query.has_interrupt_enable +
			f1a->button_query.has_multibutton_select;

	if (f1a->button_query.has_tx_rx_map) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				fhandler->full_addr.ctrl_base + mapping_offset,
				f1a->button_control.txrx_map,
				sizeof(f1a->button_control.txrx_map));
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read tx rx mapping\n",
					__func__);
			return retval;
		}

		rmi4_data->button_txrx_mapping = f1a->button_control.txrx_map;
	}

	if (!bdata->cap_button_map) {
		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: cap_button_map is NULL in board file\n",
				__func__);
	} else if (!bdata->cap_button_map->map) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Button map is missing in board file\n",
				__func__);
	} else {
		if (bdata->cap_button_map->nbuttons != f1a->max_count) {
			f1a->valid_button_count = min(f1a->max_count,
					bdata->cap_button_map->nbuttons);
		} else {
			f1a->valid_button_count = f1a->max_count;
		}

		for (ii = 0; ii < f1a->valid_button_count; ii++)
			f1a->button_map[ii] = bdata->cap_button_map->map[ii];
	}

	return 0;
}

static void synaptics_rmi4_f1a_kfree(struct synaptics_rmi4_fn *fhandler)
{
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;

	if (f1a) {
		kfree(f1a->button_control.txrx_map);
		kfree(f1a->button_data_buffer);
		kfree(f1a->button_map);
		kfree(f1a);
		fhandler->data = NULL;
	}

	return;
}

static int synaptics_rmi4_f1a_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	synaptics_rmi4_set_intr_mask(fhandler, fd, intr_count);

	retval = synaptics_rmi4_f1a_alloc_mem(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	retval = synaptics_rmi4_f1a_button_map(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	rmi4_data->button_0d_enabled = 1;

	return 0;

error_exit:
	synaptics_rmi4_f1a_kfree(fhandler);

	return retval;
}

static void synaptics_rmi4_empty_fn_list(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_fn *fhandler_temp;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry_safe(fhandler,
				fhandler_temp,
				&rmi->support_fn_list,
				link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F1A) {
				synaptics_rmi4_f1a_kfree(fhandler);
			} else {
				kfree(fhandler->extra);
				kfree(fhandler->data);
			}
			list_del(&fhandler->link);
			kfree(fhandler);
		}
	}
	INIT_LIST_HEAD(&rmi->support_fn_list);

	return;
}

static int synaptics_rmi4_check_status(struct synaptics_rmi4_data *rmi4_data,
		bool *was_in_bl_mode)
{
	int retval;
	int timeout = CHECK_STATUS_TIMEOUT_MS;
	unsigned char command = 0x01;
	unsigned char intr_status;
	struct synaptics_rmi4_f01_device_status status;

	/* Do a device reset first */
	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_cmd_base_addr,
			&command,
			sizeof(command));
	if (retval < 0)
		return retval;

	msleep(rmi4_data->hw_if->board_data->reset_delay_ms);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			status.data,
			sizeof(status.data));
	if (retval < 0)
		return retval;

	while (status.status_code == STATUS_CRC_IN_PROGRESS) {
		if (timeout > 0)
			msleep(20);
		else
			return -1;

		retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01_data_base_addr,
				status.data,
				sizeof(status.data));
		if (retval < 0)
			return retval;

		timeout -= 20;
	}

	if (timeout != CHECK_STATUS_TIMEOUT_MS)
		*was_in_bl_mode = true;

	if (status.flash_prog == 1) {
		rmi4_data->flash_prog_mode = true;
		pr_notice("%s: In flash prog mode, status = 0x%02x\n",
				__func__,
				status.status_code);
	} else {
		rmi4_data->flash_prog_mode = false;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr + 1,
			&intr_status,
			sizeof(intr_status));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read interrupt status\n",
				__func__);
		return retval;
	}

	return 0;
}

static void synaptics_rmi4_set_configured(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set configured\n",
				__func__);
		return;
	}

	rmi4_data->no_sleep_setting = device_ctrl & NO_SLEEP_ON;
	device_ctrl |= CONFIGURED;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set configured\n",
				__func__);
	}

	return;
}

static int synaptics_rmi4_alloc_fh(struct synaptics_rmi4_fn **fhandler,
		struct synaptics_rmi4_fn_desc *rmi_fd, int page_number)
{
	*fhandler = kmalloc(sizeof(**fhandler), GFP_KERNEL);
	if (!(*fhandler))
		return -ENOMEM;

	(*fhandler)->full_addr.data_base =
			(rmi_fd->data_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.ctrl_base =
			(rmi_fd->ctrl_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.cmd_base =
			(rmi_fd->cmd_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.query_base =
			(rmi_fd->query_base_addr |
			(page_number << 8));

	return 0;
}

static int synaptics_rmi4_read_configid(struct synaptics_rmi4_data *rmi4_data,
		unsigned char ctrl_base_addr)
{
	unsigned int device_config_id;

	/*
	 * We may get an error while trying to read config id if it is
	 *  not provisioned by vendor
	 */
	if (synaptics_rmi4_reg_read(rmi4_data, ctrl_base_addr,
			(unsigned char *)(&device_config_id),
			 sizeof(device_config_id)) < 0)
		dev_err(rmi4_data->pdev->dev.parent, "Failed to read device config ID from CTP\n");

	if (rmi4_data->hw_if->board_data->config_id)
		dev_info(rmi4_data->pdev->dev.parent,
			"CTP Config ID=%pI4\tDT Config ID=%pI4\n",
			&device_config_id,
			&rmi4_data->hw_if->board_data->config_id);
	else
		dev_info(rmi4_data->pdev->dev.parent,
			"CTP Config ID=%pI4\n", &device_config_id);

	return 0;
}

 /**
 * synaptics_rmi4_query_device()
 *
 * Called by synaptics_rmi4_probe().
 *
 * This funtion scans the page description table, records the offsets
 * to the register types of Function $01, sets up the function handlers
 * for Function $11 and Function $12, determines the number of interrupt
 * sources from the sensor, adds valid Functions with data inputs to the
 * Function linked list, parses information from the query registers of
 * Function $01, and enables the interrupt sources from the valid Functions
 * with data inputs.
 */
static int synaptics_rmi4_query_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii;
	unsigned char page_number;
	unsigned char intr_count;
	unsigned char f01_query[F01_STD_QUERY_LEN];
	unsigned short pdt_entry_addr;
	unsigned short intr_addr;
	bool f01found;
	bool was_in_bl_mode;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;
	unsigned char pkg_id[PACKAGE_ID_SIZE];
	rmi = &(rmi4_data->rmi4_mod_info);

rescan_pdt:
	f01found = false;
	was_in_bl_mode = false;
	intr_count = 0;
	INIT_LIST_HEAD(&rmi->support_fn_list);

	/* Scan the page description tables of the pages to service */
	for (page_number = 0; page_number < PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END;
				pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);

			retval = synaptics_rmi4_reg_read(rmi4_data,
					pdt_entry_addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			pdt_entry_addr &= ~(MASK_8BIT << 8);

			fhandler = NULL;

			if (rmi_fd.fn_number == 0) {
				dev_dbg(rmi4_data->pdev->dev.parent,
						"%s: Reached end of PDT\n",
						__func__);
				break;
			}

			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: F%02x found (page %d)\n",
					__func__, rmi_fd.fn_number,
					page_number);

			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F34:
				/*
				 * Though function F34 is an interrupt source,
				 * but it is not a data source, hence do not
				 * add its handler to support_fn_list
				 */
				synaptics_rmi4_read_configid(rmi4_data,
						 rmi_fd.ctrl_base_addr);
				break;
			case SYNAPTICS_RMI4_F01:
				if (rmi_fd.intr_src_count == 0)
					break;

				f01found = true;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f01_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;

				retval = synaptics_rmi4_check_status(rmi4_data,
						&was_in_bl_mode);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to check status\n",
							__func__);
					return retval;
				}

				if (was_in_bl_mode) {
					kfree(fhandler);
					fhandler = NULL;
					goto rescan_pdt;
				}

				if (rmi4_data->flash_prog_mode)
					goto flash_prog_mode;

				break;
			case SYNAPTICS_RMI4_F11:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f11_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;
				break;
			case SYNAPTICS_RMI4_F12:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f12_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;
				break;
			case SYNAPTICS_RMI4_F1A:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f1a_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0) {
#ifdef IGNORE_FN_INIT_FAILURE
					kfree(fhandler);
					fhandler = NULL;
#else
					return retval;
#endif
				}
				break;
			}

			/* Accumulate the interrupt count */
			intr_count += (rmi_fd.intr_src_count & MASK_3BIT);

			if (fhandler && rmi_fd.intr_src_count) {
				list_add_tail(&fhandler->link,
						&rmi->support_fn_list);
			}
		}
	}

	if (!f01found) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to find F01\n",
				__func__);
		return -EINVAL;
	}

flash_prog_mode:
	rmi4_data->num_of_intr_regs = (intr_count + 7) / 8;
	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Number of interrupt registers = %d\n",
			__func__, rmi4_data->num_of_intr_regs);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_query_base_addr,
			f01_query,
			sizeof(f01_query));
	if (retval < 0)
		return retval;

	/* RMI Version 4.0 currently supported */
	rmi->version_major = 4;
	rmi->version_minor = 0;

	rmi->manufacturer_id = f01_query[0];
	rmi->product_props = f01_query[1];
	rmi->product_info[0] = f01_query[2] & MASK_7BIT;
	rmi->product_info[1] = f01_query[3] & MASK_7BIT;
	rmi->date_code[0] = f01_query[4] & MASK_5BIT;
	rmi->date_code[1] = f01_query[5] & MASK_4BIT;
	rmi->date_code[2] = f01_query[6] & MASK_5BIT;
	rmi->tester_id = ((f01_query[7] & MASK_7BIT) << 8) |
			(f01_query[8] & MASK_7BIT);
	rmi->serial_number = ((f01_query[9] & MASK_7BIT) << 8) |
			(f01_query[10] & MASK_7BIT);
	memcpy(rmi->product_id_string, &f01_query[11], 10);

	if (rmi->manufacturer_id != 1) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Non-Synaptics device found, manufacturer ID = %d\n",
				__func__, rmi->manufacturer_id);
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_query_base_addr + F01_PACKAGE_ID_OFFSET,
			pkg_id, sizeof(pkg_id));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
			"%s: Failed to read device package id (code %d)\n",
			__func__, retval);
		return retval;
	}

	rmi->package_id = (pkg_id[1] << 8) | pkg_id[0];
	rmi->package_id_rev = (pkg_id[3] << 8) | pkg_id[2];

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_query_base_addr + F01_BUID_ID_OFFSET,
			rmi->build_id,
			sizeof(rmi->build_id));
	if (retval < 0)
		return retval;

	rmi4_data->firmware_id = (unsigned int)rmi->build_id[0] +
			(unsigned int)rmi->build_id[1] * 0x100 +
			(unsigned int)rmi->build_id[2] * 0x10000;

	memset(rmi4_data->intr_mask, 0x00, sizeof(rmi4_data->intr_mask));

	/*
	 * Map out the interrupt bit masks for the interrupt sources
	 * from the registered function handlers.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				rmi4_data->intr_mask[fhandler->intr_reg_num] |=
						fhandler->intr_mask;
			}
		}
	}

	/* Enable the interrupt sources */
	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (rmi4_data->intr_mask[ii] != 0x00) {
			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: Interrupt enable mask %d = 0x%02x\n",
					__func__, ii, rmi4_data->intr_mask[ii]);
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			retval = synaptics_rmi4_reg_write(rmi4_data,
					intr_addr,
					&(rmi4_data->intr_mask[ii]),
					sizeof(rmi4_data->intr_mask[ii]));
			if (retval < 0)
				return retval;
		}
	}

	synaptics_rmi4_set_configured(rmi4_data);

	return 0;
}

static void synaptics_rmi4_set_params(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char ii;
	struct synaptics_rmi4_f1a_handle *f1a;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (bdata->disp_maxx && bdata->disp_maxy) {
		input_set_abs_params(rmi4_data->input_dev, ABS_MT_POSITION_X,
				0, bdata->disp_maxx, 0, 0);
		input_set_abs_params(rmi4_data->input_dev, ABS_MT_POSITION_Y,
				0, bdata->disp_maxy, 0, 0);
	} else {
		input_set_abs_params(rmi4_data->input_dev, ABS_MT_POSITION_X,
				0, rmi4_data->sensor_max_x, 0, 0);
		input_set_abs_params(rmi4_data->input_dev, ABS_MT_POSITION_Y,
				0, rmi4_data->sensor_max_y, 0, 0);
	}

#ifdef REPORT_2D_W
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MAJOR, 0,
			rmi4_data->max_touch_width, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MINOR, 0,
			rmi4_data->max_touch_width, 0, 0);
#endif

#ifdef TYPE_B_PROTOCOL
	input_mt_init_slots(rmi4_data->input_dev,
			rmi4_data->num_of_fingers, 0);
#endif

	f1a = NULL;
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F1A)
				f1a = fhandler->data;
		}
	}

	if (f1a) {
		for (ii = 0; ii < f1a->valid_button_count; ii++) {
			set_bit(f1a->button_map[ii],
					rmi4_data->input_dev->keybit);
			input_set_capability(rmi4_data->input_dev,
					EV_KEY, f1a->button_map[ii]);
		}
	}

	return;
}

static int synaptics_dsx_virtual_keys_init(struct device *dev,
			struct synaptics_dsx_board_data	*rmi4_pdata)
{
	int width, height, center_x, center_y;
	int x1 = 0, x2 = 0, i, c = 0, rc = 0, border;

	vkey_buf = devm_kzalloc(dev, MAX_BUF_SIZE, GFP_KERNEL);
	if (!vkey_buf) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	border = (rmi4_pdata->panel_maxx - rmi4_pdata->disp_maxx) * 2;
	width = ((rmi4_pdata->disp_maxx -
			(border * (rmi4_pdata->virtual_key_map->nkeys - 1)))
			/ rmi4_pdata->virtual_key_map->nkeys);
	height = (rmi4_pdata->panel_maxy - rmi4_pdata->disp_maxy);
	center_y = rmi4_pdata->disp_maxy + (height / 2);
	height = height * HEIGHT_SCALE_NUM / HEIGHT_SCALE_DENOM;

	x2 -= border * BORDER_ADJUST_NUM / BORDER_ADJUST_DENOM;

	for (i = 0; i < rmi4_pdata->virtual_key_map->nkeys; i++) {
		x1 = x2 + border;
		x2 = x2 + border + width;
		center_x = x1 + (x2 - x1) / 2;
		c += snprintf(vkey_buf + c, MAX_BUF_SIZE - c,
				"%s:%d:%d:%d:%d:%d\n", VKEY_VER_CODE,
				rmi4_pdata->virtual_key_map->map[i],
				center_x, center_y, width, height);
	}

	vkey_buf[c] = '\0';

	vkey_kobj = kobject_create_and_add("board_properties", NULL);
	if (!vkey_kobj) {
		dev_err(dev, "unable to create kobject\n");
		return -ENOMEM;
	}

	rc = sysfs_create_group(vkey_kobj, &vkey_grp);
	if (rc) {
		dev_err(dev, "failed to create attributes\n");
		kobject_put(vkey_kobj);
	}

	return rc;
}

static int synaptics_dsx_get_virtual_keys(struct device *dev,
				struct property *prop, char *name,
				struct synaptics_dsx_board_data *rmi4_pdata,
				struct device_node *np)
{
	u32 num_keys;
	int rc;

	num_keys = prop->length / sizeof(u32);

	rmi4_pdata->virtual_key_map = devm_kzalloc(dev,
			sizeof(*rmi4_pdata->virtual_key_map),
			GFP_KERNEL);
	if (!rmi4_pdata->virtual_key_map)
		return -ENOMEM;

	rmi4_pdata->virtual_key_map->map = devm_kzalloc(dev,
		sizeof(*rmi4_pdata->virtual_key_map->map) *
		num_keys, GFP_KERNEL);
	if (!rmi4_pdata->virtual_key_map->map)
		return -ENOMEM;

	rc = of_property_read_u32_array(np, name,
			rmi4_pdata->virtual_key_map->map,
			num_keys);
	if (rc) {
		dev_err(dev, "Failed to read key codes\n");
		return -EINVAL;
	}
	rmi4_pdata->virtual_key_map->nkeys = num_keys;

	return 0;
}

static int synaptics_dsx_get_button_map(struct device *dev,
				struct property *prop, char *name,
				struct synaptics_dsx_board_data *rmi4_pdata,
				struct device_node *np)
{
	int rc, i;
	u32 num_buttons;
	u32 button_map[MAX_NUMBER_OF_BUTTONS];

	num_buttons = prop->length / sizeof(u32);

	rmi4_pdata->cap_button_map = devm_kzalloc(dev,
			sizeof(*rmi4_pdata->cap_button_map),
			GFP_KERNEL);
	if (!rmi4_pdata->cap_button_map)
		return -ENOMEM;

	rmi4_pdata->cap_button_map->map = devm_kzalloc(dev,
		sizeof(*rmi4_pdata->cap_button_map->map) *
		num_buttons, GFP_KERNEL);
	if (!rmi4_pdata->cap_button_map->map)
		return -ENOMEM;

	if (num_buttons <= MAX_NUMBER_OF_BUTTONS) {
		rc = of_property_read_u32_array(np,
				name, button_map, num_buttons);
		if (rc) {
			dev_err(dev, "Unable to read key codes\n");
			return rc;
		}
		for (i = 0; i < num_buttons; i++)
			rmi4_pdata->cap_button_map->map[i] =
				button_map[i];
		rmi4_pdata->cap_button_map->nbuttons = num_buttons;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int synaptics_rmi4_parse_dt_children(struct device *dev,
		struct synaptics_dsx_board_data *rmi4_pdata,
		struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_device_info *rmi = &rmi4_data->rmi4_mod_info;
	struct device_node *node = dev->of_node, *child = NULL;
	int rc = 0;
	struct synaptics_rmi4_fn *fhandler = NULL;
	struct property *prop;

	for_each_child_of_node(node, child) {
		rc = of_property_read_u32(child, "synaptics,package-id",
				&rmi4_pdata->package_id);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read package_id\n");
			return rc;
		} else if (rc == -EINVAL) {
			rmi4_pdata->package_id = 0x00;
		}

		if (rmi4_pdata->package_id) {
			if (rmi4_pdata->package_id != rmi->package_id) {
				dev_err(dev,
				"%s: Synaptics package id don't match %d %d\n",
				__func__,
				rmi4_pdata->package_id, rmi->package_id);
				/*
				 * Iterate over next child if package
				 * id does not match
				 */
				continue;
			} else if (of_property_read_bool(child,
				"synaptics,bypass-sensor-coords-check") &&
				of_find_property(child,
					"synaptics,panel-coords", NULL)) {
				/*
				 * Some unprogrammed panels from touch vendor
				 * and wrongly programmed panels from factory
				 * may return incorrect sensor coordinate range
				 * when their query registers are read, but
				 * they normally work fine in field. In such
				 * a scenario, driver can bypass the comparison
				 * of coordinate range read from sensor and read
				 * from DT and continue normal operation.
				 */
				synaptics_dsx_get_dt_coords(dev,
						"synaptics,panel-coords",
						rmi4_pdata, child);
				dev_info(dev,
					"%s Synaptics package id matches %d %d,"
					"but bypassing the comparison of sensor"
					"coordinates.\n", __func__,
					rmi4_pdata->package_id,
					rmi->package_id);
				dev_info(dev, "Pmax_x Pmax_y = %d:%d\n",
					rmi4_pdata->panel_maxx,
					rmi4_pdata->panel_maxy);
				dev_info(dev, "Smax_x Smax_y = %d:%d\n",
					rmi4_data->sensor_max_x,
					rmi4_data->sensor_max_y);
			} else {
				/*
				 * If package id read from DT matches the
				 * package id value read from touch controller,
				 * also check if sensor dimensions read from DT
				 * match those read from controller, before
				 * moving further. For this first check if touch
				 * panel coordinates are defined in DT or not.
				 */
				if (of_find_property(child,
					"synaptics,panel-coords", NULL)) {
					synaptics_dsx_get_dt_coords(dev,
						"synaptics,panel-coords",
						rmi4_pdata, child);
					dev_info(dev, "Pmax_x Pmax_y = %d:%d\n",
						rmi4_pdata->panel_maxx,
						rmi4_pdata->panel_maxy);
					dev_info(dev, "Smax_x Smax_y = %d:%d\n",
						rmi4_data->sensor_max_x,
						rmi4_data->sensor_max_y);
					if ((rmi4_pdata->panel_maxx !=
						rmi4_data->sensor_max_x) ||
						(rmi4_pdata->panel_maxy !=
						rmi4_data->sensor_max_y))
						continue;
				} else {
					dev_info(dev, "Smax_x Smax_y = %d:%d\n",
						rmi4_data->sensor_max_x,
						rmi4_data->sensor_max_y);
				}
			}
		}

		rc = synaptics_dsx_get_dt_coords(dev,
				"synaptics,display-coords", rmi4_pdata, child);
		if (rc && (rc != -EINVAL))
			return rc;

		prop = of_find_property(child, "synaptics,button-map", NULL);
		if (prop) {
			rc = synaptics_dsx_get_button_map(dev, prop,
				"synaptics,button-map", rmi4_pdata, child);
			if (rc < 0) {
				dev_err(dev, "Unable to read button map\n");
				return rc;
			}

			if (!list_empty(&rmi->support_fn_list)) {
				list_for_each_entry(fhandler,
						&rmi->support_fn_list, link) {
					if (fhandler->fn_number ==
						SYNAPTICS_RMI4_F1A)
						break;
				}
			}

			if (fhandler && fhandler->fn_number ==
					SYNAPTICS_RMI4_F1A) {
				rc = synaptics_rmi4_f1a_button_map(rmi4_data,
								fhandler);
				if (rc < 0) {
					dev_err(dev,
						"Fail to register F1A %d\n",
						rc);
					return rc;
				}
			}
		}

		prop = of_find_property(child, "synaptics,key-codes", NULL);
		if (prop) {
			rc = synaptics_dsx_get_virtual_keys(dev, prop,
				"synaptics,key-codes", rmi4_pdata, child);
			if (!rc) {
				rc = synaptics_dsx_virtual_keys_init(dev,
					rmi4_pdata);
				if (!rc)
					rmi4_data->support_vkeys = true;

			} else {
				dev_err(dev,
					"Unable to read virtual key codes\n");
				return rc;
			}
		}

		break;
	}

	return 0;
}

static int synaptics_rmi4_set_input_dev(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	int temp;

	rmi4_data->input_dev = input_allocate_device();
	if (rmi4_data->input_dev == NULL) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate input device\n",
				__func__);
		retval = -ENOMEM;
		goto err_input_device;
	}

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to query device\n",
				__func__);
		goto err_query_device;
	}

	if (rmi4_data->hw_if->board_data->detect_device) {
		retval = synaptics_rmi4_parse_dt_children(
				rmi4_data->pdev->dev.parent,
				rmi4_data->hw_if->board_data,
				rmi4_data);
		if (retval < 0)
			dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to parse device tree property\n",
				__func__);
	}

	rmi4_data->input_dev->name = PLATFORM_DRIVER_NAME;
	rmi4_data->input_dev->phys = INPUT_PHYS_NAME;
	rmi4_data->input_dev->id.product = SYNAPTICS_DSX_DRIVER_PRODUCT;
	rmi4_data->input_dev->id.version = SYNAPTICS_DSX_DRIVER_VERSION;
	rmi4_data->input_dev->dev.parent = rmi4_data->pdev->dev.parent;
	input_set_drvdata(rmi4_data->input_dev, rmi4_data);

	set_bit(EV_SYN, rmi4_data->input_dev->evbit);
	set_bit(EV_KEY, rmi4_data->input_dev->evbit);
	set_bit(EV_ABS, rmi4_data->input_dev->evbit);
	set_bit(BTN_TOUCH, rmi4_data->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, rmi4_data->input_dev->keybit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, rmi4_data->input_dev->propbit);
#endif

	if (rmi4_data->hw_if->board_data->swap_axes) {
		temp = rmi4_data->sensor_max_x;
		rmi4_data->sensor_max_x = rmi4_data->sensor_max_y;
		rmi4_data->sensor_max_y = temp;
	}

	synaptics_rmi4_set_params(rmi4_data);

	retval = input_register_device(rmi4_data->input_dev);
	if (retval) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to register input device\n",
				__func__);
		goto err_register_input;
	}

	return 0;

err_register_input:
	if (rmi4_data->support_vkeys) {
		sysfs_remove_group(vkey_kobj, &vkey_grp);
		kobject_put(vkey_kobj);
	}
err_query_device:
	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_free_device(rmi4_data->input_dev);

err_input_device:
	return retval;
}

static int synaptics_rmi4_set_gpio(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	int power_on;
	int reset_on;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	power_on = bdata->power_on_state;
	reset_on = bdata->reset_on_state;

	retval = bdata->gpio_config(
			bdata->irq_gpio,
			true, 0, 0);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to configure attention GPIO\n",
				__func__);
		goto err_gpio_irq;
	}

	if (bdata->power_gpio >= 0) {
		retval = bdata->gpio_config(
				bdata->power_gpio,
				true, 1, !power_on);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to configure power GPIO\n",
					__func__);
			goto err_gpio_power;
		}
	}

	if (bdata->reset_gpio >= 0) {
		retval = bdata->gpio_config(
				bdata->reset_gpio,
				true, 1, !reset_on);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to configure reset GPIO\n",
					__func__);
			goto err_gpio_reset;
		}
	}

	if (bdata->power_gpio >= 0) {
		gpio_set_value(bdata->power_gpio, power_on);
		msleep(bdata->power_delay_ms);
	}

	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, reset_on);
		msleep(bdata->reset_active_ms);
		gpio_set_value(bdata->reset_gpio, !reset_on);
		msleep(bdata->reset_delay_ms);
	}

	return 0;

err_gpio_reset:
	if (bdata->power_gpio >= 0) {
		bdata->gpio_config(
				bdata->power_gpio,
				false, 0, 0);
	}

err_gpio_power:
	bdata->gpio_config(
			bdata->irq_gpio,
			false, 0, 0);

err_gpio_irq:
	return retval;
}

static int synaptics_dsx_pinctrl_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	rmi4_data->ts_pinctrl = devm_pinctrl_get((rmi4_data->pdev->dev.parent));
	if (IS_ERR_OR_NULL(rmi4_data->ts_pinctrl)) {
		retval = PTR_ERR(rmi4_data->ts_pinctrl);
		dev_dbg(rmi4_data->pdev->dev.parent,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	rmi4_data->pinctrl_state_active
		= pinctrl_lookup_state(rmi4_data->ts_pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(rmi4_data->pinctrl_state_active)) {
		retval = PTR_ERR(rmi4_data->pinctrl_state_active);
		dev_err(rmi4_data->pdev->dev.parent,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	rmi4_data->pinctrl_state_suspend
		= pinctrl_lookup_state(rmi4_data->ts_pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(rmi4_data->pinctrl_state_suspend)) {
		retval = PTR_ERR(rmi4_data->pinctrl_state_suspend);
		dev_dbg(rmi4_data->pdev->dev.parent,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	rmi4_data->pinctrl_state_release
		= pinctrl_lookup_state(rmi4_data->ts_pinctrl, "pmx_ts_release");
	if (IS_ERR_OR_NULL(rmi4_data->pinctrl_state_release)) {
		retval = PTR_ERR(rmi4_data->pinctrl_state_release);
		dev_dbg(rmi4_data->pdev->dev.parent,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_RELEASE, retval);
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(rmi4_data->ts_pinctrl);
err_pinctrl_get:
	rmi4_data->ts_pinctrl = NULL;
	return retval;
}

static int synaptics_dsx_gpio_configure(struct synaptics_rmi4_data *rmi4_data,
					bool on)
{
	int retval = 0;
	struct synaptics_rmi4_device_info *rmi;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;
	rmi = &(rmi4_data->rmi4_mod_info);

	if (on) {
		if (gpio_is_valid(bdata->irq_gpio)) {
			/* configure touchscreen irq gpio */
			retval = gpio_request(bdata->irq_gpio,
				"rmi4_irq_gpio");
			if (retval) {
				dev_err(rmi4_data->pdev->dev.parent,
					"unable to request gpio [%d]\n",
					bdata->irq_gpio);
				goto err_irq_gpio_req;
			}
			retval = gpio_direction_input(bdata->irq_gpio);
			if (retval) {
				dev_err(rmi4_data->pdev->dev.parent,
					"unable to set dir for gpio[%d]\n",
					bdata->irq_gpio);
				goto err_irq_gpio_dir;
			}
		} else {
			dev_err(rmi4_data->pdev->dev.parent,
				"irq gpio not provided\n");
			goto err_irq_gpio_req;
		}

		if (gpio_is_valid(bdata->reset_gpio)) {
			/* configure touchscreen reset out gpio */
			retval = gpio_request(bdata->reset_gpio,
					"rmi4_reset_gpio");
			if (retval) {
				dev_err(rmi4_data->pdev->dev.parent,
					"unable to request gpio [%d]\n",
					bdata->reset_gpio);
				goto err_irq_gpio_dir;
			}

			retval = gpio_direction_output(bdata->reset_gpio, 1);
			if (retval) {
				dev_err(rmi4_data->pdev->dev.parent,
					"unable to set dir for gpio [%d]\n",
					bdata->reset_gpio);
				goto err_reset_gpio_dir;
			}

			gpio_set_value(bdata->reset_gpio, 1);
			msleep(bdata->reset_delay_ms);
		}

		return 0;
	} else {
		if (bdata->disable_gpios) {
			if (gpio_is_valid(bdata->irq_gpio))
				gpio_free(bdata->irq_gpio);
			if (gpio_is_valid(bdata->reset_gpio)) {
				/*
				 * This is intended to save leakage current
				 * only. Even if the call(gpio_direction_input)
				 * fails, only leakage current will be more but
				 * functionality will not be affected.
				 */
				if (rmi->package_id ==
						SYNA_S332U_PACKAGE_ID &&
					rmi->package_id_rev ==
						SYNA_S332U_PACKAGE_ID_REV) {
					gpio_set_value(bdata->
						reset_gpio,
						0);
				} else {
					retval = gpio_direction_input(
						bdata->reset_gpio);
					if (retval) {
						dev_err(rmi4_data->pdev->
						dev.parent,
						"unable to set direction for gpio [%d]\n",
						bdata->irq_gpio);
					}
				}
				gpio_free(bdata->reset_gpio);
			}
		}

		return 0;
	}

err_reset_gpio_dir:
	if (gpio_is_valid(bdata->reset_gpio))
		gpio_free(bdata->reset_gpio);
err_irq_gpio_dir:
	if (gpio_is_valid(bdata->irq_gpio))
		gpio_free(bdata->irq_gpio);
err_irq_gpio_req:
	return retval;
}

static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char ii;

#ifdef TYPE_B_PROTOCOL
	for (ii = 0; ii < rmi4_data->num_of_fingers; ii++) {
		input_mt_slot(rmi4_data->input_dev, ii);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, 0);
	}
#endif
	input_report_key(rmi4_data->input_dev,
			BTN_TOUCH, 0);
	input_report_key(rmi4_data->input_dev,
			BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
	input_mt_sync(rmi4_data->input_dev);
#endif
	input_sync(rmi4_data->input_dev);

	rmi4_data->fingers_on_2d = false;

	return 0;
}

static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii;
	unsigned short intr_addr;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));

	synaptics_rmi4_free_fingers(rmi4_data);

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F12) {
				synaptics_rmi4_f12_set_coords(rmi4_data,
								fhandler);
				synaptics_rmi4_f12_set_enables(rmi4_data, 0);
				break;
			} else if (fhandler->fn_number == SYNAPTICS_RMI4_F11) {
				synaptics_rmi4_f11_set_coords(rmi4_data,
								fhandler);
				break;
			}

		}
	}

	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (rmi4_data->intr_mask[ii] != 0x00) {
			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: Interrupt enable mask %d = 0x%02x\n",
					__func__, ii, rmi4_data->intr_mask[ii]);
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			retval = synaptics_rmi4_reg_write(rmi4_data,
					intr_addr,
					&(rmi4_data->intr_mask[ii]),
					sizeof(rmi4_data->intr_mask[ii]));
			if (retval < 0)
				goto exit;
		}
	}

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->reinit != NULL)
				exp_fhandler->exp_fn->reinit(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	synaptics_rmi4_set_configured(rmi4_data);

	retval = 0;

exit:
	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
	return retval;
}

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	int temp;
	unsigned char command = 0x01;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));

	rmi4_data->touch_stopped = true;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_cmd_base_addr,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
		return retval;
	}

	msleep(rmi4_data->hw_if->board_data->reset_delay_ms);

	synaptics_rmi4_free_fingers(rmi4_data);

	synaptics_rmi4_empty_fn_list(rmi4_data);

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to query device\n",
				__func__);
		mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
		return retval;
	}

	if (rmi4_data->hw_if->board_data->swap_axes) {
		temp = rmi4_data->sensor_max_x;
		rmi4_data->sensor_max_x = rmi4_data->sensor_max_y;
		rmi4_data->sensor_max_y = temp;
	}

	synaptics_rmi4_set_params(rmi4_data);

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->reset != NULL)
				exp_fhandler->exp_fn->reset(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	rmi4_data->touch_stopped = false;

	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));

	return 0;
}

/**
* synaptics_rmi4_exp_fn_work()
*
* Called by the kernel at the scheduled time.
*
* This function is a work thread that checks for the insertion and
* removal of other expansion Function modules such as rmi_dev and calls
* their initialization and removal callback functions accordingly.
*/
static void synaptics_rmi4_exp_fn_work(struct work_struct *work)
{
	int retval;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler_temp;
	struct synaptics_rmi4_data *rmi4_data = exp_data.rmi4_data;

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry_safe(exp_fhandler,
				exp_fhandler_temp,
				&exp_data.list,
				link) {
			if ((exp_fhandler->exp_fn->init != NULL) &&
					exp_fhandler->insert) {
				retval = exp_fhandler->exp_fn->init(rmi4_data);
				if (retval < 0) {
					list_del(&exp_fhandler->link);
					kfree(exp_fhandler);
				} else {
					exp_fhandler->insert = false;
				}
			} else if ((exp_fhandler->exp_fn->remove != NULL) &&
					exp_fhandler->remove) {
				exp_fhandler->exp_fn->remove(rmi4_data);
				list_del(&exp_fhandler->link);
				kfree(exp_fhandler);
			}
		}
	}
	mutex_unlock(&exp_data.mutex);

	return;
}

/**
* synaptics_rmi4_dsx_new_function()
*
* Called by other expansion Function modules in their module init and
* module exit functions.
*
* This function is used by other expansion Function modules such as
* rmi_dev to register themselves with the driver by providing their
* initialization and removal callback function pointers so that they
* can be inserted or removed dynamically at module init and exit times,
* respectively.
*/
void synaptics_rmi4_dsx_new_function(struct synaptics_rmi4_exp_fn *exp_fn,
		bool insert)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;

	if (!exp_data.initialized) {
		mutex_init(&exp_data.mutex);
		INIT_LIST_HEAD(&exp_data.list);
		exp_data.initialized = true;
	}

	mutex_lock(&exp_data.mutex);
	if (insert) {
		exp_fhandler = kzalloc(sizeof(*exp_fhandler), GFP_KERNEL);
		if (!exp_fhandler) {
			pr_err("%s: Failed to alloc mem for expansion function\n",
					__func__);
			goto exit;
		}
		exp_fhandler->exp_fn = exp_fn;
		exp_fhandler->insert = true;
		exp_fhandler->remove = false;
		list_add_tail(&exp_fhandler->link, &exp_data.list);
	} else if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link) {
			if (exp_fhandler->exp_fn->fn_type == exp_fn->fn_type) {
				exp_fhandler->insert = false;
				exp_fhandler->remove = true;
				goto exit;
			}
		}
	}

exit:
	mutex_unlock(&exp_data.mutex);

	if (exp_data.queue_work) {
		queue_delayed_work(exp_data.workqueue,
				&exp_data.work,
				msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));
	}

	return;
}
EXPORT_SYMBOL(synaptics_rmi4_dsx_new_function);

static int synaptics_dsx_regulator_configure(struct synaptics_rmi4_data
			*rmi4_data)
{
	int retval;
	rmi4_data->regulator_vdd = regulator_get(rmi4_data->pdev->dev.parent,
			"vdd");
	if (IS_ERR(rmi4_data->regulator_vdd)) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to get regulator vdd\n",
				__func__);
		retval = PTR_ERR(rmi4_data->regulator_vdd);
		return retval;
	}
	rmi4_data->regulator_avdd = regulator_get(rmi4_data->pdev->dev.parent,
			"avdd");
	if (IS_ERR(rmi4_data->regulator_avdd)) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to get regulator avdd\n",
				__func__);
		retval = PTR_ERR(rmi4_data->regulator_avdd);
		regulator_put(rmi4_data->regulator_vdd);
		return retval;
	}

	return 0;
};

static int synaptics_dsx_regulator_enable(struct synaptics_rmi4_data
			*rmi4_data, bool on)
{
	int retval;

	if (on) {
		retval = regulator_enable(rmi4_data->regulator_vdd);
		if (retval) {
			dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to enable regulator vdd\n",
				__func__);
			return retval;
		}
		retval = regulator_enable(rmi4_data->regulator_avdd);
		if (retval) {
			dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to enable regulator avdd\n",
				__func__);
			regulator_disable(rmi4_data->regulator_vdd);
			return retval;
		}
		msleep(rmi4_data->hw_if->board_data->power_delay_ms);
	} else {
		regulator_disable(rmi4_data->regulator_vdd);
		regulator_disable(rmi4_data->regulator_avdd);
	}

	return 0;
}

 /**
 * synaptics_rmi4_probe()
 *
 * Called by the kernel when an association with an I2C device of the
 * same name is made (after doing i2c_add_driver).
 *
 * This funtion allocates and initializes the resources for the driver
 * as an input driver, turns on the power to the sensor, queries the
 * sensor for its supported Functions and characteristics, registers
 * the driver to the input subsystem, sets up the interrupt, handles
 * the registration of the early_suspend and late_resume functions,
 * and creates a work queue for detection of other expansion Function
 * modules.
 */
static int synaptics_rmi4_probe(struct platform_device *pdev)
{
	int retval, len;
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data;
	const struct synaptics_dsx_hw_interface *hw_if;
	const struct synaptics_dsx_board_data *bdata;
	struct dentry *temp;

	hw_if = pdev->dev.platform_data;
	if (!hw_if) {
		dev_err(&pdev->dev,
				"%s: No hardware interface found\n",
				__func__);
		return -EINVAL;
	}

	bdata = hw_if->board_data;
	if (!bdata) {
		dev_err(&pdev->dev,
				"%s: No board data found\n",
				__func__);
		return -EINVAL;
	}

	rmi4_data = kzalloc(sizeof(*rmi4_data), GFP_KERNEL);
	if (!rmi4_data) {
		dev_err(&pdev->dev,
				"%s: Failed to alloc mem for rmi4_data\n",
				__func__);
		return -ENOMEM;
	}

	rmi4_data->pdev = pdev;
	rmi4_data->current_page = MASK_8BIT;
	rmi4_data->hw_if = hw_if;
	rmi4_data->touch_stopped = false;
	rmi4_data->sensor_sleep = false;
	rmi4_data->irq_enabled = false;
	rmi4_data->fw_updating = false;
	rmi4_data->fingers_on_2d = false;
	rmi4_data->update_coords = true;

	rmi4_data->write_buf = devm_kzalloc(&pdev->dev, I2C_WRITE_BUF_MAX_LEN,
					GFP_KERNEL);
	if (!rmi4_data->write_buf)
		return -ENOMEM;
	rmi4_data->write_buf_len = I2C_WRITE_BUF_MAX_LEN;

	rmi4_data->irq_enable = synaptics_rmi4_irq_enable;
	rmi4_data->reset_device = synaptics_rmi4_reset_device;

	mutex_init(&(rmi4_data->rmi4_io_ctrl_mutex));
	mutex_init(&(rmi4_data->rmi4_reset_mutex));

	retval = synaptics_dsx_regulator_configure(rmi4_data);
	if (retval) {
		dev_err(&pdev->dev,
			"%s: regulator configuration failed\n", __func__);
		goto err_regulator_configure;
	}
	retval = synaptics_dsx_regulator_enable(rmi4_data, true);
	if (retval) {
		dev_err(&pdev->dev,
			"%s: regulator enable failed\n", __func__);
		goto err_regulator_enable;
	}

	platform_set_drvdata(pdev, rmi4_data);

	if (bdata->gpio_config) {
		retval = synaptics_rmi4_set_gpio(rmi4_data);
		if (retval < 0) {
			dev_err(&pdev->dev,
				"%s: Failed to set up GPIO's\n",
				__func__);
			goto err_set_gpio;
		}
	} else {
		retval = synaptics_dsx_pinctrl_init(rmi4_data);
		if (!retval && rmi4_data->ts_pinctrl) {
			/*
			* Pinctrl handle is optional. If pinctrl handle is found
			* let pins to be configured in active state. If not
			* found continue further without error.
			*/
			retval = pinctrl_select_state(rmi4_data->ts_pinctrl,
					rmi4_data->pinctrl_state_active);
			if (retval < 0) {
				dev_err(&pdev->dev,
					"%s: Failed to select %s pinstate %d\n",
					__func__, PINCTRL_STATE_ACTIVE, retval);
			}
		}

		retval = synaptics_dsx_gpio_configure(rmi4_data, true);
		if (retval < 0) {
			dev_err(&pdev->dev,
					"%s: Failed to set up GPIO's\n",
					__func__);
			goto err_config_gpio;
		}
	}

	if (bdata->fw_name) {
		len = strlen(bdata->fw_name);
		if (len > SYNA_FW_NAME_MAX_LEN - 1) {
			dev_err(&pdev->dev, "Invalid firmware name\n");
			goto err_set_input_dev;
		}

		strlcpy(rmi4_data->fw_name, bdata->fw_name, len + 1);
	}

	retval = synaptics_rmi4_set_input_dev(rmi4_data);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to set up input device\n",
				__func__);
		goto err_set_input_dev;
	}

#ifdef CONFIG_FB
	INIT_WORK(&rmi4_data->fb_notify_work, fb_notify_resume_work);
	rmi4_data->fb_notif.notifier_call = fb_notifier_callback;

	retval = fb_register_client(&rmi4_data->fb_notif);
	if (retval)
		dev_err(rmi4_data->pdev->dev.parent,
			"Unable to register fb_notifier: %d\n", retval);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	rmi4_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	rmi4_data->early_suspend.suspend = synaptics_rmi4_early_suspend;
	rmi4_data->early_suspend.resume = synaptics_rmi4_late_resume;
	register_early_suspend(&rmi4_data->early_suspend);
#endif

	rmi4_data->irq = gpio_to_irq(bdata->irq_gpio);

	retval = synaptics_rmi4_irq_enable(rmi4_data, true);
	if (retval < 0) {
		dev_err(&pdev->dev,
				"%s: Failed to enable attention interrupt\n",
				__func__);
		goto err_enable_irq;
	}

	if (!exp_data.initialized) {
		mutex_init(&exp_data.mutex);
		INIT_LIST_HEAD(&exp_data.list);
		exp_data.initialized = true;
	}

	exp_data.workqueue = create_singlethread_workqueue("dsx_exp_workqueue");
	INIT_DELAYED_WORK(&exp_data.work, synaptics_rmi4_exp_fn_work);
	exp_data.rmi4_data = rmi4_data;
	exp_data.queue_work = true;
	queue_delayed_work(exp_data.workqueue,
			&exp_data.work,
			msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));

	rmi4_data->dir = debugfs_create_dir(DEBUGFS_DIR_NAME, NULL);
	if (rmi4_data->dir == NULL || IS_ERR(rmi4_data->dir)) {
		retval = rmi4_data->dir ? PTR_ERR(rmi4_data->dir) : -EIO;
		dev_err(&pdev->dev,
			"%s: Failed to create debugfs directory, rc = %d\n",
			__func__, retval);
		goto err_create_debugfs_dir;
	}

	temp = debugfs_create_file("suspend", S_IRUSR | S_IWUSR, rmi4_data->dir,
					rmi4_data, &debug_suspend_fops);
	if (temp == NULL || IS_ERR(temp)) {
		retval = temp ? PTR_ERR(temp) : -EIO;
		dev_err(&pdev->dev,
			"%s: Failed to create suspend debugfs file, rc = %d\n",
			__func__, retval);
		goto err_create_debugfs_file;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(&pdev->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			goto err_sysfs;
		}
	}

	synaptics_secure_touch_init(rmi4_data);
	synaptics_secure_touch_stop(rmi4_data, 1);

	return retval;

err_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}
err_create_debugfs_file:
	debugfs_remove_recursive(rmi4_data->dir);
err_create_debugfs_dir:
	cancel_delayed_work_sync(&exp_data.work);
	if (exp_data.workqueue != NULL) {
		flush_workqueue(exp_data.workqueue);
		destroy_workqueue(exp_data.workqueue);
	}
	synaptics_rmi4_irq_enable(rmi4_data, false);
	free_irq(rmi4_data->irq, rmi4_data);

err_enable_irq:
#if defined(CONFIG_FB)
	fb_unregister_client(&rmi4_data->fb_notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&rmi4_data->early_suspend);
#endif

	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;

err_set_input_dev:
	if (bdata->gpio_config) {
		bdata->gpio_config(
				bdata->irq_gpio,
				false, 0, 0);

		if (bdata->reset_gpio >= 0) {
			bdata->gpio_config(
					bdata->reset_gpio,
					false, 0, 0);
		}

		if (bdata->power_gpio >= 0) {
			bdata->gpio_config(
					bdata->power_gpio,
					false, 0, 0);
		}
	} else {
		synaptics_dsx_gpio_configure(rmi4_data, false);
	}
err_config_gpio:
	if (rmi4_data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(rmi4_data->pinctrl_state_release)) {
			devm_pinctrl_put(rmi4_data->ts_pinctrl);
			rmi4_data->ts_pinctrl = NULL;
		} else {
			retval = pinctrl_select_state(
				rmi4_data->ts_pinctrl,
				rmi4_data->pinctrl_state_release);
			if (retval)
				dev_err(&pdev->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
		}
	}

err_set_gpio:
	regulator_disable(rmi4_data->regulator_vdd);
	regulator_disable(rmi4_data->regulator_avdd);
err_regulator_enable:
	regulator_put(rmi4_data->regulator_vdd);
	regulator_put(rmi4_data->regulator_avdd);
err_regulator_configure:
	kfree(rmi4_data);

	return retval;
}

 /**
 * synaptics_rmi4_remove()
 *
 * Called by the kernel when the association with an I2C device of the
 * same name is broken (when the driver is unloaded).
 *
 * This funtion terminates the work queue, stops sensor data acquisition,
 * frees the interrupt, unregisters the driver from the input subsystem,
 * turns off the power to the sensor, and frees other allocated resources.
 */
static int synaptics_rmi4_remove(struct platform_device *pdev)
{
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data = platform_get_drvdata(pdev);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;
	int err;

	if (rmi4_data->support_vkeys) {
		sysfs_remove_group(vkey_kobj, &vkey_grp);
		kobject_put(vkey_kobj);
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	debugfs_remove_recursive(rmi4_data->dir);
	cancel_delayed_work_sync(&exp_data.work);
	flush_workqueue(exp_data.workqueue);
	destroy_workqueue(exp_data.workqueue);

	synaptics_rmi4_irq_enable(rmi4_data, false);

#if defined(CONFIG_FB)
	fb_unregister_client(&rmi4_data->fb_notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&rmi4_data->early_suspend);
#endif

	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;

	if (bdata->gpio_config) {
		bdata->gpio_config(
				bdata->irq_gpio,
				false, 0, 0);

		if (bdata->reset_gpio >= 0) {
			bdata->gpio_config(
					bdata->reset_gpio,
					false, 0, 0);
		}

		if (bdata->power_gpio >= 0) {
			bdata->gpio_config(
					bdata->power_gpio,
					false, 0, 0);
		}
	} else {
		synaptics_dsx_gpio_configure(rmi4_data, false);
		if (rmi4_data->ts_pinctrl) {
			if (IS_ERR_OR_NULL(rmi4_data->pinctrl_state_release)) {
				devm_pinctrl_put(rmi4_data->ts_pinctrl);
				rmi4_data->ts_pinctrl = NULL;
			} else {
				err = pinctrl_select_state(
					rmi4_data->ts_pinctrl,
					rmi4_data->pinctrl_state_release);
				if (err)
					dev_err(&pdev->dev,
						"Failed to select release pinctrl state %d\n",
						err);
			}
		}
	}

	if (rmi4_data->regulator_vdd) {
		regulator_disable(rmi4_data->regulator_vdd);
		regulator_put(rmi4_data->regulator_vdd);
	}

	if (rmi4_data->regulator_avdd) {
		regulator_disable(rmi4_data->regulator_avdd);
		regulator_put(rmi4_data->regulator_avdd);
	}

	kfree(rmi4_data);

	return 0;
}

#if defined(CONFIG_FB)
static void fb_notify_resume_work(struct work_struct *work)
{
	struct synaptics_rmi4_data *rmi4_data =
		container_of(work, struct synaptics_rmi4_data, fb_notify_work);
	synaptics_rmi4_resume(&(rmi4_data->input_dev->dev));
}

static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct synaptics_rmi4_data *rmi4_data =
		container_of(self, struct synaptics_rmi4_data, fb_notif);

	if (evdata && evdata->data && rmi4_data) {
		blank = evdata->data;
		if (rmi4_data->hw_if->board_data->resume_in_workqueue) {
			if (event == FB_EARLY_EVENT_BLANK) {
				synaptics_secure_touch_stop(rmi4_data, 0);
				if (*blank == FB_BLANK_UNBLANK)
					schedule_work(
						&(rmi4_data->fb_notify_work));
			} else if (event == FB_EVENT_BLANK &&
					*blank == FB_BLANK_POWERDOWN) {
					flush_work(
						&(rmi4_data->fb_notify_work));
					synaptics_rmi4_suspend(
						&(rmi4_data->input_dev->dev));
			}
		} else {
			if (event == FB_EARLY_EVENT_BLANK) {
				synaptics_secure_touch_stop(rmi4_data, 0);
			} else if (event == FB_EVENT_BLANK) {
				if (*blank == FB_BLANK_UNBLANK)
					synaptics_rmi4_resume(
						&(rmi4_data->input_dev->dev));
				else if (*blank == FB_BLANK_POWERDOWN)
					synaptics_rmi4_suspend(
						&(rmi4_data->input_dev->dev));
			}
		}
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
 /**
 * synaptics_rmi4_early_suspend()
 *
 * Called by the kernel during the early suspend phase when the system
 * enters suspend.
 *
 * This function calls synaptics_rmi4_sensor_sleep() to stop finger
 * data acquisition and put the sensor to sleep.
 */
static void synaptics_rmi4_early_suspend(struct early_suspend *h)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(h, struct synaptics_rmi4_data,
			early_suspend);

	if (rmi4_data->stay_awake) {
		rmi4_data->staying_awake = true;
		return;
	} else {
		rmi4_data->staying_awake = false;
	}

	synaptics_secure_touch_stop(rmi4_data, 0);

	rmi4_data->touch_stopped = true;
	synaptics_rmi4_irq_enable(rmi4_data, false);
	synaptics_rmi4_sensor_sleep(rmi4_data);
	synaptics_rmi4_free_fingers(rmi4_data);

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->early_suspend != NULL)
				exp_fhandler->exp_fn->early_suspend(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	if (rmi4_data->full_pm_cycle)
		synaptics_rmi4_suspend(&(rmi4_data->input_dev->dev));

	return;
}

 /**
 * synaptics_rmi4_late_resume()
 *
 * Called by the kernel during the late resume phase when the system
 * wakes up from suspend.
 *
 * This function goes through the sensor wake process if the system wakes
 * up from early suspend (without going into suspend).
 */
static void synaptics_rmi4_late_resume(struct early_suspend *h)
{
	int retval;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(h, struct synaptics_rmi4_data,
			early_suspend);

	if (rmi4_data->staying_awake)
		return;

	synaptics_secure_touch_stop(rmi4_data, 0);

	if (rmi4_data->full_pm_cycle)
		synaptics_rmi4_resume(&(rmi4_data->input_dev->dev));

	if (rmi4_data->sensor_sleep == true) {
		synaptics_rmi4_sensor_wake(rmi4_data);
		synaptics_rmi4_irq_enable(rmi4_data, true);
		retval = synaptics_rmi4_reinit_device(rmi4_data);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to reinit device\n",
					__func__);
		}
	}

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->late_resume != NULL)
				exp_fhandler->exp_fn->late_resume(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	rmi4_data->touch_stopped = false;

	return;
}
#endif

#ifdef CONFIG_PM
 /**
 * synaptics_rmi4_sensor_sleep()
 *
 * Called by synaptics_rmi4_early_suspend() and synaptics_rmi4_suspend().
 *
 * This function stops finger data acquisition and puts the sensor to sleep.
 */
static void synaptics_rmi4_sensor_sleep(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to enter sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = false;
		return;
	}

	device_ctrl = (device_ctrl & ~MASK_3BIT);
	device_ctrl = (device_ctrl | NO_SLEEP_OFF | SENSOR_SLEEP);

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to enter sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = false;
		return;
	} else {
		rmi4_data->sensor_sleep = true;
	}

	return;
}

 /**
 * synaptics_rmi4_sensor_wake()
 *
 * Called by synaptics_rmi4_resume() and synaptics_rmi4_late_resume().
 *
 * This function wakes the sensor from sleep.
 */
static void synaptics_rmi4_sensor_wake(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;
	unsigned char no_sleep_setting = rmi4_data->no_sleep_setting;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to wake from sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = true;
		return;
	}

	device_ctrl = (device_ctrl & ~MASK_3BIT);
	device_ctrl = (device_ctrl | no_sleep_setting | NORMAL_OPERATION);

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to wake from sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = true;
		return;
	} else {
		rmi4_data->sensor_sleep = false;
	}

	return;
}

 /**
 * synaptics_rmi4_suspend()
 *
 * Called by the kernel during the suspend phase when the system
 * enters suspend.
 *
 * This function stops finger data acquisition and puts the sensor to
 * sleep (if not already done so during the early suspend phase),
 * disables the interrupt, and turns off the power to the sensor.
 */
static int synaptics_rmi4_suspend(struct device *dev)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;
	int retval;

	if (rmi4_data->stay_awake) {
		rmi4_data->staying_awake = true;
		return 0;
	} else {
		rmi4_data->staying_awake = false;
	}

	if (rmi4_data->suspended) {
		dev_info(dev, "Already in suspend state\n");
		return 0;
	}

	synaptics_secure_touch_stop(rmi4_data, 1);

	if (!rmi4_data->fw_updating) {
		if (!rmi4_data->sensor_sleep) {
			rmi4_data->touch_stopped = true;
			synaptics_rmi4_irq_enable(rmi4_data, false);
			synaptics_rmi4_sensor_sleep(rmi4_data);
			synaptics_rmi4_free_fingers(rmi4_data);
		}

		mutex_lock(&exp_data.mutex);
		if (!list_empty(&exp_data.list)) {
			list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->suspend != NULL)
				exp_fhandler->exp_fn->suspend(rmi4_data);
		}
		mutex_unlock(&exp_data.mutex);

		retval = synaptics_dsx_regulator_enable(rmi4_data, false);
		if (retval < 0) {
			dev_err(dev, "failed to enter low power mode\n");
			goto err_lpm_regulator;
		}
	} else {
		dev_err(dev,
			"Firmware updating, cannot go into suspend mode\n");
		return 0;
	}

	if (bdata->disable_gpios) {
		if (rmi4_data->ts_pinctrl) {
			retval = pinctrl_select_state(rmi4_data->ts_pinctrl,
					rmi4_data->pinctrl_state_suspend);
			if (retval < 0) {
				dev_err(dev, "Cannot get idle pinctrl state\n");
				goto err_pinctrl_select_suspend;
			}
		}

		retval = synaptics_dsx_gpio_configure(rmi4_data, false);
		if (retval < 0) {
			dev_err(dev, "failed to put gpios in suspend state\n");
			goto err_gpio_configure;
		}
	}

	rmi4_data->suspended = true;

	return 0;

err_gpio_configure:
	if (rmi4_data->ts_pinctrl) {
		retval = pinctrl_select_state(rmi4_data->ts_pinctrl,
					rmi4_data->pinctrl_state_active);
		if (retval < 0)
			dev_err(dev, "Cannot get default pinctrl state\n");
	}
err_pinctrl_select_suspend:
	synaptics_dsx_regulator_enable(rmi4_data, true);
err_lpm_regulator:
	if (rmi4_data->sensor_sleep) {
		synaptics_rmi4_sensor_wake(rmi4_data);
		synaptics_rmi4_irq_enable(rmi4_data, true);
		rmi4_data->touch_stopped = false;
	}

	return retval;
}

 /**
 * synaptics_rmi4_resume()
 *
 * Called by the kernel during the resume phase when the system
 * wakes up from suspend.
 *
 * This function turns on the power to the sensor, wakes the sensor
 * from sleep, enables the interrupt, and starts finger data
 * acquisition.
 */
static int synaptics_rmi4_resume(struct device *dev)
{
	int retval;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	rmi = &(rmi4_data->rmi4_mod_info);
	if (rmi->package_id == SYNA_S332U_PACKAGE_ID &&
			rmi->package_id_rev == SYNA_S332U_PACKAGE_ID_REV) {
		synaptics_rmi4_reset_device(rmi4_data);
	}

	if (rmi4_data->staying_awake)
		return 0;

	if (!rmi4_data->suspended)
		return 0;

	synaptics_secure_touch_stop(rmi4_data, 1);

	synaptics_dsx_regulator_enable(rmi4_data, true);

	if (bdata->disable_gpios) {
		if (rmi4_data->ts_pinctrl) {
			retval = pinctrl_select_state(rmi4_data->ts_pinctrl,
					rmi4_data->pinctrl_state_active);
			if (retval < 0)
				dev_err(dev, "Cannot get default pinctrl state\n");
		}

		retval = synaptics_dsx_gpio_configure(rmi4_data, true);
		if (retval < 0)
			dev_err(dev, "Failed to put gpios in active state\n");
	}

	synaptics_rmi4_sensor_wake(rmi4_data);
	retval = synaptics_rmi4_reinit_device(rmi4_data);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to reinit device\n",
				__func__);
		return retval;
	}

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->resume != NULL)
				exp_fhandler->exp_fn->resume(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	rmi4_data->touch_stopped = false;
	rmi4_data->suspended = false;

	synaptics_rmi4_irq_enable(rmi4_data, true);

	return 0;
}

static const struct dev_pm_ops synaptics_rmi4_dev_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend = synaptics_rmi4_suspend,
	.resume  = synaptics_rmi4_resume,
#endif
};
#else
static int synaptics_rmi4_suspend(struct device *dev)
{
	dev_err(dev, "PM not supported\n");
	return -EINVAL;
}

static int synaptics_rmi4_resume(struct device *dev)
{
	dev_err(dev, "PM not supported\n");
	return -EINVAL;
}
#endif

static struct platform_driver synaptics_rmi4_driver = {
	.driver = {
		.name = PLATFORM_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &synaptics_rmi4_dev_pm_ops,
#endif
	},
	.probe = synaptics_rmi4_probe,
	.remove = synaptics_rmi4_remove,
};

 /**
 * synaptics_rmi4_init()
 *
 * Called by the kernel during do_initcalls (if built-in)
 * or when the driver is loaded (if a module).
 *
 * This function registers the driver to the I2C subsystem.
 *
 */
static int __init synaptics_rmi4_init(void)
{
	int retval;

	retval = synaptics_rmi4_bus_init();
	if (retval)
		return retval;

	return platform_driver_register(&synaptics_rmi4_driver);
}

 /**
 * synaptics_rmi4_exit()
 *
 * Called by the kernel when the driver is unloaded.
 *
 * This funtion unregisters the driver from the I2C subsystem.
 *
 */
static void __exit synaptics_rmi4_exit(void)
{
	platform_driver_unregister(&synaptics_rmi4_driver);

	synaptics_rmi4_bus_exit();

	return;
}

module_init(synaptics_rmi4_init);
module_exit(synaptics_rmi4_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX I2C Touch Driver");
MODULE_LICENSE("GPL v2");
