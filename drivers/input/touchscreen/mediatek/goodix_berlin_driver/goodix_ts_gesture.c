/*
 * Goodix Gesture Module
 *
 * Copyright (C) 2019 - 2020 Goodix, Inc.
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
#include <linux/string.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/input/mt.h>
#include "goodix_ts_core.h"


#define GOODIX_GESTURE_DOUBLE_TAP		0xCC
#define GOODIX_GESTURE_SINGLE_TAP		0x4C
#define GOODIX_GESTURE_FOD_DOWN			0x46
#define GOODIX_GESTURE_FOD_UP			0x55

/*
 * struct gesture_module - gesture module data
 * @registered: module register state
 * @sysfs_node_created: sysfs node state
 * @gesture_type: valid gesture type, each bit represent one gesture type
 * @gesture_data: store latest gesture code get from irq event
 * @gesture_ts_cmd: gesture command data
 */
struct gesture_module {
	atomic_t registered;
	struct goodix_ts_core *ts_core;
	struct goodix_ext_module module;
};

static struct gesture_module *gsx_gesture; /*allocated in gesture init module*/
static bool module_initialized;

static ssize_t gsx_double_type_show(struct goodix_ext_module *module,
		char *buf)
{
	struct gesture_module *gsx = module->priv_data;
	unsigned char type = gsx->ts_core->gesture_type;

	if (!gsx)
		return -EIO;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	return sprintf(buf, "%s\n",
			(type & GESTURE_DOUBLE_TAP) ? "enable" : "disable");
}

static ssize_t gsx_double_type_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct gesture_module *gsx = module->priv_data;

	if (!gsx)
		return -EIO;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	if (buf[0] == '1') {
		ts_info("enable double tap");
		gsx->ts_core->gesture_type |= GESTURE_DOUBLE_TAP;
	} else if (buf[0] == '0') {
		ts_info("disable double tap");
		gsx->ts_core->gesture_type &= ~GESTURE_DOUBLE_TAP;
	} else
		ts_err("invalid cmd[%d]", buf[0]);

	return count;
}

static ssize_t gsx_single_type_show(struct goodix_ext_module *module,
		char *buf)
{
	struct gesture_module *gsx = module->priv_data;
	unsigned char type = gsx->ts_core->gesture_type;

	if (!gsx)
		return -EIO;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	return sprintf(buf, "%s\n",
			(type & GESTURE_SINGLE_TAP) ? "enable" : "disable");
}

static ssize_t gsx_single_type_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct gesture_module *gsx = module->priv_data;

	if (!gsx)
		return -EIO;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	if (buf[0] == '1') {
		ts_info("enable single tap");
		gsx->ts_core->gesture_type |= GESTURE_SINGLE_TAP;
	} else if (buf[0] == '0') {
		ts_info("disable single tap");
		gsx->ts_core->gesture_type &= ~GESTURE_SINGLE_TAP;
	} else
		ts_err("invalid cmd[%d]", buf[0]);

	return count;
}

static ssize_t gsx_fod_type_show(struct goodix_ext_module *module,
		char *buf)
{
	struct gesture_module *gsx = module->priv_data;
	unsigned char type = gsx->ts_core->gesture_type;

	if (!gsx)
		return -EIO;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	return sprintf(buf, "%s\n",
			(type & GESTURE_FOD_PRESS) ? "enable" : "disable");
}

static ssize_t gsx_fod_type_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct gesture_module *gsx = module->priv_data;

	if (!gsx)
		return -EIO;

	if (atomic_read(&gsx->registered) == 0) {
		ts_err("gesture module is not registered");
		return 0;
	}

	if (buf[0] == '1') {
		ts_info("enable fod");
		gsx->ts_core->gesture_type |= GESTURE_FOD_PRESS;
	} else if (buf[0] == '0') {
		ts_info("disable fod");
		gsx->ts_core->gesture_type &= ~GESTURE_FOD_PRESS;
	} else
		ts_err("invalid cmd[%d]", buf[0]);

	return count;
}


const struct goodix_ext_attribute gesture_attrs[] = {
	__EXTMOD_ATTR(double_en, 0664,
			gsx_double_type_show, gsx_double_type_store),
	__EXTMOD_ATTR(single_en, 0664,
			gsx_single_type_show, gsx_single_type_store),
	__EXTMOD_ATTR(fod_en, 0664,
			gsx_fod_type_show, gsx_fod_type_store),
};

static int gsx_gesture_init(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	struct gesture_module *gsx = module->priv_data;

	if (!cd || !cd->hw_ops->gesture) {
		ts_err("gesture unsupported");
		return -EINVAL;
	}

	gsx->ts_core = cd;
	gsx->ts_core->gesture_type = 0;
	atomic_set(&gsx->registered, 1);

	return 0;
}

static int gsx_gesture_exit(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	struct gesture_module *gsx = module->priv_data;

	if (!cd || !cd->hw_ops->gesture) {
		ts_err("gesture unsupported");
		return -EINVAL;
	}

	atomic_set(&gsx->registered, 0);

	return 0;
}

/**
 * gsx_gesture_ist - Gesture Irq handle
 * This functions is excuted when interrupt happended and
 * ic in doze mode.
 *
 * @cd: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_CANCEL_IRQEVT  stop execute
 */
static int gsx_gesture_ist(struct goodix_ts_core *cd,
	struct goodix_ext_module *module)
{
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	struct goodix_ts_event gs_event = {0};
	int fodx, fody, overlay_area;
	int ret;

	if (atomic_read(&cd->suspended) == 0 || cd->gesture_type == 0)
		return EVT_CONTINUE;

	ret = hw_ops->event_handler(cd, &gs_event);
	if (ret) {
		ts_err("failed get gesture data");
		goto re_send_ges_cmd;
	}

	if (!(gs_event.event_type & EVENT_GESTURE)) {
		ts_err("invalid event type: 0x%x",
			cd->ts_event.event_type);
		goto re_send_ges_cmd;
	}

	switch (gs_event.gesture_type) {
	case GOODIX_GESTURE_SINGLE_TAP:
		if (cd->gesture_type & GESTURE_SINGLE_TAP) {
			ts_info("get SINGLE-TAP gesture");
			input_report_key(cd->input_dev, KEY_WAKEUP, 1);
			// input_report_key(cd->input_dev, KEY_GOTO, 1);
			input_sync(cd->input_dev);
			input_report_key(cd->input_dev, KEY_WAKEUP, 0);
			// input_report_key(cd->input_dev, KEY_GOTO, 0);
			input_sync(cd->input_dev);
		} else {
			ts_debug("not enable SINGLE-TAP");
		}
		break;
	case GOODIX_GESTURE_DOUBLE_TAP:
		if (cd->gesture_type & GESTURE_DOUBLE_TAP) {
			ts_info("get DOUBLE-TAP gesture");
			input_report_key(cd->input_dev, KEY_WAKEUP, 1);
			input_sync(cd->input_dev);
			input_report_key(cd->input_dev, KEY_WAKEUP, 0);
			input_sync(cd->input_dev);
		} else {
			ts_debug("not enable DOUBLE-TAP");
		}
		break;
	case GOODIX_GESTURE_FOD_DOWN:
		if (cd->gesture_type & GESTURE_FOD_PRESS) {
			ts_info("get FOD-DOWN gesture");
			fodx = le16_to_cpup((__le16 *)gs_event.gesture_data);
			fody = le16_to_cpup((__le16 *)(gs_event.gesture_data + 2));
			overlay_area = gs_event.gesture_data[4];
			ts_debug("fodx:%d fody:%d overlay_area:%d", fodx, fody, overlay_area);
			input_report_key(cd->input_dev, BTN_TOUCH, 1);
			input_mt_slot(cd->input_dev, 0);
			input_mt_report_slot_state(cd->input_dev, MT_TOOL_FINGER, 1);
			input_report_abs(cd->input_dev, ABS_MT_POSITION_X, fodx);
			input_report_abs(cd->input_dev, ABS_MT_POSITION_Y, fody);
			input_report_abs(cd->input_dev, ABS_MT_WIDTH_MAJOR, overlay_area);
			input_sync(cd->input_dev);
		} else {
			ts_debug("not enable FOD-DOWN");
		}
		break;
	case GOODIX_GESTURE_FOD_UP:
		if (cd->gesture_type & GESTURE_FOD_PRESS) {
			ts_info("get FOD-UP gesture");
			fodx = le16_to_cpup((__le16 *)gs_event.gesture_data);
			fody = le16_to_cpup((__le16 *)(gs_event.gesture_data + 2));
			overlay_area = gs_event.gesture_data[4];
			input_report_key(cd->input_dev, BTN_TOUCH, 0);
			input_mt_slot(cd->input_dev, 0);
			input_mt_report_slot_state(cd->input_dev,
					MT_TOOL_FINGER, 0);
			input_sync(cd->input_dev);
		} else {
			ts_debug("not enable FOD-UP");
		}
		break;
	default:
		ts_err("not support gesture type[%02X]", gs_event.gesture_type);
		break;
	}

re_send_ges_cmd:
	if (hw_ops->gesture(cd, 0))
		ts_info("warning: failed re_send gesture cmd");
	return EVT_CANCEL_IRQEVT;
}

/**
 * gsx_gesture_before_suspend - execute gesture suspend routine
 * This functions is excuted to set ic into doze mode
 *
 * @cd: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_IRQCANCLED  stop execute
 */
static int gsx_gesture_before_suspend(struct goodix_ts_core *cd,
	struct goodix_ext_module *module)
{
	int ret;
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	if (cd->gesture_type == 0)
		return EVT_CONTINUE;

	ret = hw_ops->gesture(cd, 0);
	if (ret)
		ts_err("failed enter gesture mode");
	else
		ts_info("enter gesture mode, type[0x%02X]", cd->gesture_type);

	hw_ops->irq_enable(cd, true);
	enable_irq_wake(cd->irq);

	return EVT_CANCEL_SUSPEND;
}

static int gsx_gesture_before_resume(struct goodix_ts_core *cd,
	struct goodix_ext_module *module)
{
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	if (cd->gesture_type == 0)
		return EVT_CONTINUE;

	disable_irq_wake(cd->irq);
	hw_ops->reset(cd, GOODIX_NORMAL_RESET_DELAY_MS);

	return EVT_CANCEL_RESUME;
}

static struct goodix_ext_module_funcs gsx_gesture_funcs = {
	.irq_event = gsx_gesture_ist,
	.init = gsx_gesture_init,
	.exit = gsx_gesture_exit,
	.before_suspend = gsx_gesture_before_suspend,
	.before_resume = gsx_gesture_before_resume,
};

int gesture_module_init(void)
{
	int ret;
	int i;
	struct kobject *def_kobj = goodix_get_default_kobj();
	struct kobj_type *def_kobj_type = goodix_get_default_ktype();

	gsx_gesture = kzalloc(sizeof(struct gesture_module), GFP_KERNEL);
	if (!gsx_gesture)
		return -ENOMEM;

	gsx_gesture->module.funcs = &gsx_gesture_funcs;
	gsx_gesture->module.priority = EXTMOD_PRIO_GESTURE;
	gsx_gesture->module.name = "Goodix_gsx_gesture";
	gsx_gesture->module.priv_data = gsx_gesture;

	atomic_set(&gsx_gesture->registered, 0);

	/* gesture sysfs init */
	ret = kobject_init_and_add(&gsx_gesture->module.kobj,
			def_kobj_type, def_kobj, "gesture");
	if (ret) {
		ts_err("failed create gesture sysfs node!");
		goto err_out;
	}

	for (i = 0; i < ARRAY_SIZE(gesture_attrs) && !ret; i++)
		ret = sysfs_create_file(&gsx_gesture->module.kobj,
				&gesture_attrs[i].attr);
	if (ret) {
		ts_err("failed create gst sysfs files");
		while (--i >= 0)
			sysfs_remove_file(&gsx_gesture->module.kobj,
					&gesture_attrs[i].attr);

		kobject_put(&gsx_gesture->module.kobj);
		goto err_out;
	}

	module_initialized = true;
	goodix_register_ext_module_no_wait(&gsx_gesture->module);
	ts_info("gesture module init success");

	return 0;

err_out:
	ts_err("gesture module init failed!");
	kfree(gsx_gesture);
	return ret;
}

void gesture_module_exit(void)
{
	int i;

	ts_info("gesture module exit");
	if (!module_initialized)
		return;

	goodix_unregister_ext_module(&gsx_gesture->module);

	/* deinit sysfs */
	for (i = 0; i < ARRAY_SIZE(gesture_attrs); i++)
		sysfs_remove_file(&gsx_gesture->module.kobj,
					&gesture_attrs[i].attr);

	kobject_put(&gsx_gesture->module.kobj);
	kfree(gsx_gesture);
	module_initialized = false;
}
