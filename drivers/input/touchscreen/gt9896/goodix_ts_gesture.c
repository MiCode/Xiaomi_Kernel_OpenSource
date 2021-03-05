/*
 * Goodix Gesture Module
 *
 * Copyright (C) 2019 - 2020 Goodix, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include "goodix_ts_core.h"

#define GSX_GESTURE_CMD					0x08

#define QUERYBIT(longlong, bit)			(!!(longlong[bit/8] & (1 << bit%8)))

#define GSX_MAX_KEY_DATA_LEN			64
#define GSX_KEY_DATA_LEN				37
#define GSX_KEY_DATA_LEN_YS				41
#define GSX_KEY_DATA_HEAD_LEN_YS		8
#define GSX_GESTURE_TYPE_LEN			32

extern struct goodix_ts_core *goodix_core_data;

/*
 * struct gesture_module - gesture module data
 * @registered: module register state
 * @sysfs_node_created: sysfs node state
 * @gesture_type: store valid gesture type,each bit stand for a gesture
 * @gesture_data: gesture data
 * @gesture_ts_cmd: gesture command data
 */
struct gesture_module {
	atomic_t registered;
	unsigned int kobj_initialized;
	rwlock_t rwlock;
	unsigned char gesture_type[GSX_GESTURE_TYPE_LEN];
	unsigned char gesture_data[GSX_MAX_KEY_DATA_LEN];
	struct goodix_ext_module module;
	struct goodix_ts_cmd cmd;
};

static int gsx_enter_gesture_mode(struct goodix_ts_device *ts_dev);
static struct gesture_module *gsx_gesture; /*allocated in gesture init module*/

/**
 * gsx_gesture_type_show - show valid gesture type
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to output buffer
 * Returns >=0 - succeed,< 0 - failed
 */
static ssize_t gsx_gesture_type_show(struct goodix_ext_module *module,
				char *buf)
{
	int count = 0, i, ret = 0;
	unsigned char *type;

	if (atomic_read(&gsx_gesture->registered) != 1) {
		ts_info("Gesture module not register!");
		return -EPERM;
	}
	type = kzalloc(256, GFP_KERNEL);
	if (!type)
		return -ENOMEM;
	read_lock(&gsx_gesture->rwlock);
	for (i = 0; i < 256; i++) {
		if (QUERYBIT(gsx_gesture->gesture_type, i)) {
			type[count] = i;
			count++;
		}
	}
	type[count] = '\0';
	if (count > 0)
		ret = scnprintf(buf, PAGE_SIZE, "%s", type);
	read_unlock(&gsx_gesture->rwlock);

	kfree(type);
	return ret;
}

/**
 * gsx_gesture_type_store - set vailed gesture
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to valid gesture type
 * @count: length of buf
 * Returns >0 - valid gestures, < 0 - failed
 */
static ssize_t gsx_gesture_type_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	int i;

	if (count <= 0 || count > 256 || buf == NULL) {
		ts_err("Parameter error");
		return -EINVAL;
	}

	write_lock(&gsx_gesture->rwlock);
	memset(gsx_gesture->gesture_type, 0, GSX_GESTURE_TYPE_LEN);
	for (i = 0; i < count; i++)
		gsx_gesture->gesture_type[buf[i]/8] |= (0x1 << buf[i]%8);
	write_unlock(&gsx_gesture->rwlock);

	return count;
}

static ssize_t gsx_gesture_enable_show(struct goodix_ext_module *module,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 atomic_read(&gsx_gesture->registered));
}

static ssize_t gsx_gesture_enable_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	unsigned int tmp;
	int ret;

	if (sscanf(buf, "%u", &tmp) != 1) {
		ts_info("Parameter illegal");
		return -EINVAL;
	}
	ts_debug("Tmp value =%d", tmp);

	if (tmp == 1) {
		if (atomic_read(&gsx_gesture->registered)) {
			ts_debug("Gesture module has aready registered");
			return count;
		}
		ret = goodix_register_ext_module(&gsx_gesture->module);
		if (!ret) {
			ts_info("Gesture module registered!");
			atomic_set(&gsx_gesture->registered, 1);
		} else {
			atomic_set(&gsx_gesture->registered, 0);
			ts_err("Gesture module register failed");
		}
	} else if (tmp == 0) {
		if (!atomic_read(&gsx_gesture->registered)) {
			ts_debug("Gesture module has aready unregistered");
			return count;
		}
		ts_debug("Start unregistered gesture module");
		ret = goodix_unregister_ext_module(&gsx_gesture->module);
		if (!ret) {
			atomic_set(&gsx_gesture->registered, 0);
			ts_info("Gesture module unregistered success");
		} else {
			atomic_set(&gsx_gesture->registered, 1);
			ts_info("Gesture module unregistered failed");
		}
	} else {
		ts_err("Parameter error!");
		return -EINVAL;
	}
	return count;
}

static ssize_t gsx_gesture_data_show(struct goodix_ext_module *module,
				char *buf)
{
	ssize_t count;

	if (atomic_read(&gsx_gesture->registered) != 1) {
		ts_info("Gesture module not register!");
		return -EPERM;
	}
	if (!buf || !*(gsx_gesture->gesture_data)) {
		ts_info("Parameter error!");
		return -EPERM;
	}
	read_lock(&gsx_gesture->rwlock);

	count = scnprintf(buf, PAGE_SIZE, "Previous gesture type:0x%x\n",
			  gsx_gesture->gesture_data[2]);
	read_unlock(&gsx_gesture->rwlock);

	return count;
}

const struct goodix_ext_attribute gesture_attrs[] = {
	__EXTMOD_ATTR(type, 0666, gsx_gesture_type_show,
		gsx_gesture_type_store),
	__EXTMOD_ATTR(enable, 0666, gsx_gesture_enable_show,
		gsx_gesture_enable_store),
	__EXTMOD_ATTR(data, 0444, gsx_gesture_data_show, NULL)
};

static int gsx_enter_gesture_mode(struct goodix_ts_device *ts_dev)
{
	u8 mode_cmd;

	mode_cmd = ~(0xF8 | goodix_core_data->gesture_enabled);

	if (!ts_dev->reg.command) {
		ts_err("command reg can not be null");
		return -EINVAL;
	}
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE) {
		gsx_gesture->cmd.cmd_reg = ts_dev->reg.command;
		gsx_gesture->cmd.length = 5;
		gsx_gesture->cmd.cmds[0] = GSX_GESTURE_CMD;
		gsx_gesture->cmd.cmds[1] = mode_cmd;
		gsx_gesture->cmd.cmds[2] = 0x0;
		gsx_gesture->cmd.cmds[3] = 0x0;
		gsx_gesture->cmd.cmds[4] = GSX_GESTURE_CMD + mode_cmd;
		gsx_gesture->cmd.initialized = 1;

	} else {
		gsx_gesture->cmd.cmd_reg = ts_dev->reg.command;
		gsx_gesture->cmd.length = 3;
		gsx_gesture->cmd.cmds[0] = GSX_GESTURE_CMD;
		gsx_gesture->cmd.cmds[1] = 0x0;
		gsx_gesture->cmd.cmds[2] = 0 - GSX_GESTURE_CMD;
		gsx_gesture->cmd.initialized = 1;
		ts_info("enter gesture mode success.");
	}


	return ts_dev->hw_ops->send_cmd(ts_dev, &gsx_gesture->cmd);
}

static int gsx_gesture_init(struct goodix_ts_core *core_data,
		struct goodix_ext_module *module)
{
	int i, ret = -EINVAL;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	if (!core_data || !ts_dev->hw_ops->write || !ts_dev->hw_ops->read) {
		ts_err("Register gesture module failed, ts_core unsupported");
		goto exit_gesture_init;
	}

	memset(gsx_gesture->gesture_type, 0, GSX_GESTURE_TYPE_LEN);
	memset(gsx_gesture->gesture_data, 0xff,
	       sizeof(gsx_gesture->gesture_data));

	ts_debug("Set gesture type manually");
	/* set all bit to 1 to enable all gesture wakeup */
	memset(gsx_gesture->gesture_type, 0xff, GSX_GESTURE_TYPE_LEN);

	if (gsx_gesture->kobj_initialized) {
		ret = 0;
		goto exit_gesture_init;
	}

	ret = kobject_init_and_add(&module->kobj, goodix_get_default_ktype(),
			&core_data->pdev->dev.kobj, "gesture");

	if (ret) {
		ts_err("Create gesture sysfs node error!");
		goto exit_gesture_init;
	}

	ret = 0;
	for (i = 0; i < ARRAY_SIZE(gesture_attrs) && !ret; i++)
		ret = sysfs_create_file(&module->kobj, &gesture_attrs[i].attr);
	if (ret) {
		ts_err("failed create gst sysfs files");
		while (--i >= 0)
			sysfs_remove_file(&module->kobj, &gesture_attrs[i].attr);

		kobject_put(&module->kobj);
		goto exit_gesture_init;
	}

	gsx_gesture->kobj_initialized = 1;

exit_gesture_init:
	return ret;
}

static int gsx_gesture_exit(struct goodix_ts_core *core_data,
		struct goodix_ext_module *module)
{
	atomic_set(&gsx_gesture->registered, 0);
	return 0;
}

/**
 * gsx_gesture_ist - Gesture Irq handle
 * This functions is excuted when interrupt happended and
 * ic in doze mode.
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_CANCEL_IRQEVT  stop execute
 */
static int gsx_gesture_ist(struct goodix_ts_core *core_data,
	struct goodix_ext_module *module)
{
	int ret;
	static int FP_Event_Gesture;
	int x, y, area, overlapping_area;
	int key_data_len = 0;
	u8 clear_reg = 0, checksum = 0, gsx_type = 0;
	u8 temp_data[GSX_MAX_KEY_DATA_LEN];
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	if (atomic_read(&core_data->suspended) == 0)
		return EVT_CONTINUE;

	if (!ts_dev->reg.gesture) {
		ts_err("gesture reg can't be null");
		return EVT_CONTINUE;
	}

	mutex_lock(&ts_dev->report_mutex);
	core_data->start_time = ktime_get_boot_ns();

	/* get ic gesture state*/
	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE)
		key_data_len = GSX_KEY_DATA_LEN;
	else
		key_data_len = GSX_KEY_DATA_LEN;

	ret = ts_dev->hw_ops->read_trans(ts_dev, ts_dev->reg.gesture,
		temp_data, key_data_len);
	if (ret < 0 || ((temp_data[0] & GOODIX_GESTURE_EVENT)  == 0)) {
		ts_err("invalid gesture event, ret=%d, temp_data[0]=0x%x",
			ret, temp_data[0]);
		goto re_send_ges_cmd;
	}

	if ((temp_data[0] & 0x08)  != 0) {
		FP_Event_Gesture = 1;
		/*ts_debug("FP_Event_Gesture = %d", FP_Event_Gesture);*/
	}

	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE)
		checksum = checksum_u8_ys(temp_data, GSX_KEY_DATA_HEAD_LEN_YS);
	else
		checksum = checksum_u8(temp_data, key_data_len);
	if (checksum) {
		ts_err("Gesture data checksum error:0x%x", checksum);
		ts_info("Gesture data %*ph",
			(int)sizeof(temp_data), temp_data);
		goto re_send_ges_cmd;
	}
	ts_notice("Gesture data %*ph", 5, temp_data);

	write_lock(&gsx_gesture->rwlock);
	memcpy(gsx_gesture->gesture_data, temp_data, key_data_len);
	write_unlock(&gsx_gesture->rwlock);

	if (ts_dev->ic_type == IC_TYPE_YELLOWSTONE)
		gsx_type = temp_data[4];
	else
		gsx_type = temp_data[2];

	if ((core_data->fod_status > 0 && core_data->fod_status != 100) ||
			core_data->aod_status) {
		if ((FP_Event_Gesture == 1) && (gsx_type == 0x46) && !core_data->sleep_finger) {
			x = temp_data[9] | (temp_data[8] << 8);
			y = temp_data[11] | (temp_data[10] << 8);
			overlapping_area = temp_data[12];
			area = temp_data[13];

			input_mt_slot(core_data->input_dev, 0);
			input_mt_report_slot_state(core_data->input_dev, MT_TOOL_FINGER, true);
			input_report_key(core_data->input_dev, BTN_INFO, 1);
			input_report_key(core_data->input_dev, BTN_TOUCH, 1);
			input_report_key(core_data->input_dev, BTN_TOOL_FINGER, 1);
			input_report_abs(core_data->input_dev, ABS_MT_TOOL_TYPE, MT_TOOL_FINGER);
			input_report_abs(core_data->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(core_data->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(core_data->input_dev, ABS_MT_WIDTH_MAJOR, overlapping_area);
			/*input_report_abs(core_data->input_dev, ABS_MT_TOUCH_MINOR, area);*/

			core_data->fod_pressed = true;
			__set_bit(0, &core_data->touch_id);

			/*wait for report key event*/
			FP_Event_Gesture = 0;
			input_sync(core_data->input_dev);
			ts_info("Gesture Report_0x152 DOWN_suspend, fod_area=%d.", overlapping_area);
		}

		if ((FP_Event_Gesture == 1) && (gsx_type == 0x4c)) {
			/*wait for report key event*/
			FP_Event_Gesture = 0;
			input_report_key(core_data->input_dev, KEY_GOTO, 1);
			input_sync(core_data->input_dev);
			input_report_key(core_data->input_dev, KEY_GOTO, 0);
			input_sync(core_data->input_dev);
			ts_info("Gesture report L");
		}

		if ((FP_Event_Gesture == 1) && (gsx_type == 0x55)) {
			if (core_data->fod_pressed) {
				ts_info("Gesture Report_0x152 UP_suspend, fod_area = %d. ", temp_data[12]);
				input_mt_slot(core_data->input_dev, 0);
				input_mt_report_slot_state(core_data->input_dev, MT_TOOL_FINGER, false);
				input_report_abs(core_data->input_dev, ABS_MT_WIDTH_MAJOR, 0);
				input_report_key(core_data->input_dev, BTN_INFO, 0);
				input_report_key(core_data->input_dev, BTN_TOUCH, 0);
				input_report_key(core_data->input_dev, BTN_TOOL_FINGER, 0);
				input_sync(core_data->input_dev);
				__clear_bit(0, &core_data->touch_id);
				core_data->fod_pressed = false;
				core_data->sleep_finger = false;
			}
		}

		write_lock(&gsx_gesture->rwlock);
		memcpy(gsx_gesture->gesture_data, temp_data, sizeof(temp_data));
		write_unlock(&gsx_gesture->rwlock);
	}

	if (gsx_type == 0xcc && core_data->double_wakeup) {
		ts_info("Gesture report DoubleClick, resume IC");
		input_report_key(core_data->input_dev, KEY_WAKEUP, 1);
		input_sync(core_data->input_dev);
		input_report_key(core_data->input_dev, KEY_WAKEUP, 0);
		input_sync(core_data->input_dev);
		goto gesture_ist_exit;
	}

	if (QUERYBIT(gsx_gesture->gesture_type, gsx_type)) {
		/* do resume routine */
		ts_info("Gesture match success, gst_type=0x%02X", gsx_type);
		goto gesture_ist_exit;
	} else {
		ts_info("Unsupported gesture:%x", gsx_type);
	}
#if 0
	if (QUERYBIT(gsx_gesture->gesture_type, gsx_type)) {
		/* do resume routine */
		ts_info("Gesture match success, resume IC");
		input_report_key(core_data->input_dev, KEY_POWER, 1);
		input_sync(core_data->input_dev);
		input_report_key(core_data->input_dev, KEY_POWER, 0);
		input_sync(core_data->input_dev);
		goto gesture_ist_exit;
	} else {
		ts_info("Unsupported gesture:%x", temp_data[2]);
	}
#endif
re_send_ges_cmd:
	if (gsx_enter_gesture_mode(core_data->ts_dev))
		ts_info("warning: failed re_send gesture cmd\n");
gesture_ist_exit:
	ts_dev->hw_ops->write_trans(ts_dev, ts_dev->reg.gesture,
				&clear_reg, 1);
	core_data->end_time = ktime_get_boot_ns();
	ts_notice("Gesture execution time = %d ns", core_data->end_time - core_data->start_time);
	mutex_unlock(&ts_dev->report_mutex);
	return EVT_CANCEL_IRQEVT;
}

/**
 * gsx_gesture_before_suspend - execute gesture suspend routine
 * This functions is excuted to set ic into doze mode
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_IRQCANCLED  stop execute
 */
static int gsx_gesture_before_suspend(struct goodix_ts_core *core_data,
	struct goodix_ext_module *module)
{
	int ret;
	const struct goodix_ts_hw_ops *hw_ops = core_data->ts_dev->hw_ops;
	/*struct goodix_ts_cmd *gesture_cmd = &gsx_gesture->cmd;*/

	if (!core_data->gesture_enabled || hw_ops == NULL) {
		ts_err("enter %s\n", __func__);
		atomic_set(&core_data->suspended, 1);
		return EVT_CONTINUE;
	}
/*
	if (!gesture_cmd->initialized || hw_ops == NULL) {
		ts_err("Uninitialized doze command or hw_ops");
		return 0;
	}
*/
	ret = gsx_enter_gesture_mode(core_data->ts_dev);
	if (ret != 0) {
		ts_err("failed enter gesture mode");
		return 0;
	}
	ts_info("Set IC in gesture mode");
	atomic_set(&core_data->suspended, 1);
	return EVT_CANCEL_SUSPEND;
}

static struct goodix_ext_module_funcs gsx_gesture_funcs = {
	.irq_event = gsx_gesture_ist,
	.init = gsx_gesture_init,
	.exit = gsx_gesture_exit,
	.before_suspend = gsx_gesture_before_suspend
};

static int __init goodix_gsx_gesture_init(void)
{
	/* initialize core_data->ts_dev->gesture_cmd */
	int result;
	ts_info("gesture module init");
	gsx_gesture = kzalloc(sizeof(struct gesture_module), GFP_KERNEL);
	if (!gsx_gesture)
		result = -ENOMEM;
	gsx_gesture->module.funcs = &gsx_gesture_funcs;
	gsx_gesture->module.priority = EXTMOD_PRIO_GESTURE;
	gsx_gesture->module.name = "Goodix_gsx_gesture";
	gsx_gesture->module.priv_data = gsx_gesture;
	gsx_gesture->kobj_initialized = 0;
	atomic_set(&gsx_gesture->registered, 0);
	rwlock_init(&gsx_gesture->rwlock);
	result = goodix_register_ext_module(&(gsx_gesture->module));
	if (result == 0)
		atomic_set(&gsx_gesture->registered, 1);

	return result;
}

static void __exit goodix_gsx_gesture_exit(void)
{
	int i, ret;
	ts_info("gesture module exit");
	if (atomic_read(&gsx_gesture->registered)) {
		ret = goodix_unregister_ext_module(&gsx_gesture->module);
		atomic_set(&gsx_gesture->registered, 0);
	}
	if (gsx_gesture->kobj_initialized) {
		for (i = 0; i < ARRAY_SIZE(gesture_attrs); i++)
			sysfs_remove_file(&gsx_gesture->module.kobj,
					&gesture_attrs[i].attr);

		kobject_put(&gsx_gesture->module.kobj);
	}

	kfree(gsx_gesture);
}

module_init(goodix_gsx_gesture_init);
module_exit(goodix_gsx_gesture_exit);

MODULE_DESCRIPTION("Goodix gsx Touchscreen Gesture Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
