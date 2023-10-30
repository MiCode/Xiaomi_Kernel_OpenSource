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

#define QUERYBIT(longlong, bit) (!!(longlong[bit/8] & (1 << bit%8)))

#define GSX_GESTURE_TYPE_LEN	32
#define TYPE_B_PROTOCOL

static int  FP_Event_Gesture;



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
	rwlock_t rwlock;
	u8 gesture_type[GSX_GESTURE_TYPE_LEN];
	u8 gesture_data;
	struct goodix_ext_module module;
};

static struct gesture_module *gsx_gesture; /*allocated in gesture init module*/
static bool module_initialized;


int goodix_gesture_enable(int enable)
{
	int ret = 0;

	if (!module_initialized)
		return 0;		

	if (enable) {
		if (atomic_read(&gsx_gesture->registered))
			ts_info("gesture module has been already registered");
		else
			ret = goodix_register_ext_module_no_wait(&gsx_gesture->module);
	} else {
		if (!atomic_read(&gsx_gesture->registered))
			ts_info("gesture module has been already unregistered");
		else
			ret = goodix_unregister_ext_module(&gsx_gesture->module);
	}

	return ret;
}

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

	type = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!type)
		return -ENOMEM;
	read_lock(&gsx_gesture->rwlock);
	for (i = 0; i < 256; i++) {
		if (QUERYBIT(gsx_gesture->gesture_type, i)) {
			count += scnprintf(type + count,
					   PAGE_SIZE, "%02x,", i);
		}
	}
	if (count > 0)
		ret = scnprintf(buf, PAGE_SIZE, "%s\n", type);
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
	bool val;
	int ret;

	ret = strtobool(buf, &val);
	if (ret < 0)
		return ret;

	if (val) {
		ret = goodix_gesture_enable(1);
		return ret ? ret : count;
	} else {
		ret = goodix_gesture_enable(0);
		return ret ? ret : count;
	}
}

static ssize_t gsx_gesture_data_show(struct goodix_ext_module *module,
				char *buf)
{
	ssize_t count;

	read_lock(&gsx_gesture->rwlock);
	count = scnprintf(buf, PAGE_SIZE, "gesture type code:0x%x\n",
			  gsx_gesture->gesture_data);
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

static int gsx_gesture_init(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	if (!cd || !cd->hw_ops->gesture) {
		ts_err("gesture unsupported");
		return -EINVAL;
	}

	ts_info("gesture switch: ON");
	ts_debug("enable all gesture type");
	/* set all bit to 1 to enable all gesture wakeup */
	memset(gsx_gesture->gesture_type, 0xff, GSX_GESTURE_TYPE_LEN);
	atomic_set(&gsx_gesture->registered, 1);

	return 0;
}

static int gsx_gesture_exit(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	if (!cd || !cd->hw_ops->gesture) {
		ts_err("gesture unsupported");
		return -EINVAL;
	}

	ts_info("gesture switch: OFF");
	ts_debug("disable all gesture type");
	memset(gsx_gesture->gesture_type, 0x00, GSX_GESTURE_TYPE_LEN);
	atomic_set(&gsx_gesture->registered, 0);
	
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
	int ret;
	int key_value;
	unsigned int fodx,fody;
	unsigned int overlay_area;
	u8 gesture_data[32];

	if (atomic_read(&cd->suspended) == 0)
		return EVT_CONTINUE;

	mutex_lock(&cd->report_mutex);

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

	memcpy(gesture_data,gs_event.touch_data.tmp_data,32*sizeof(u8));
	if ((gesture_data[0] & 0x08)  != 0)
		FP_Event_Gesture = 1;

	if (cd->fod_status && (FP_Event_Gesture == 1) &&
		(gs_event.gesture_type== 0x46)) {
		fodx = gesture_data[8] | (gesture_data[9] << 8);
		fody = gesture_data[10] | (gesture_data[11] << 8);
		overlay_area=gesture_data[12];
		ts_debug("gesture coordinate 0x%x,0x%x,0x%x",
                            fodx,fody,overlay_area);
			//input_report_key(cd->input_dev, BTN_INFO, 1);
			input_sync(cd->input_dev);
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(cd->input_dev, 0);
			input_mt_report_slot_state(cd->input_dev,
					MT_TOOL_FINGER, 1);
#endif
			input_report_key(cd->input_dev, BTN_TOUCH, 1);
			input_report_key(cd->input_dev, BTN_TOOL_FINGER, 1);
			input_report_abs(cd->input_dev,ABS_MT_POSITION_X,fodx);
			input_report_abs(cd->input_dev,ABS_MT_POSITION_Y,fody);
			input_report_abs(cd->input_dev, ABS_MT_WIDTH_MAJOR,overlay_area);
			input_report_abs(cd->input_dev, ABS_MT_WIDTH_MINOR,overlay_area);
			input_sync(cd->input_dev);
			//mi_disp_lhbm_fod_set_finger_event(0, 1, true);
			cd->fod_finger = true;
			FP_Event_Gesture = 0;
			goto re_send_ges_cmd;
	}
	if  ( (FP_Event_Gesture == 1) && (gs_event.gesture_type== 0x55)){
		if (cd->fod_finger) {
			ts_info("fod finger is %d",cd->fod_finger);
			cd->fod_finger = false;
			//input_report_key(cd->input_dev, BTN_INFO, 0);
			input_report_abs(cd->input_dev, ABS_MT_WIDTH_MAJOR, 0);
			input_report_abs(cd->input_dev, ABS_MT_WIDTH_MINOR, 0);
			input_sync(cd->input_dev);
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(cd->input_dev, 0);
			input_mt_report_slot_state(cd->input_dev,
					MT_TOOL_FINGER, 0);
#endif
			input_report_key(cd->input_dev, BTN_TOUCH, 0);
			input_report_key(cd->input_dev, BTN_TOOL_FINGER, 0);
			input_sync(cd->input_dev);
			//mi_disp_lhbm_fod_set_finger_event(0, 0, true);
		}
		goto re_send_ges_cmd;
	}
	if (QUERYBIT(gsx_gesture->gesture_type, gs_event.gesture_type)) {
		gsx_gesture->gesture_data = gs_event.gesture_type;
		/* do resume routine */
		ts_info("GTP got valid gesture type 0x%x", gs_event.gesture_type);
		if (cd->double_wakeup && gs_event.gesture_type == 0xcc) {
			ts_info("GTP gesture report double tap");
			key_value = KEY_WAKEUP;
		}
		if (cd->fod_icon_status && gs_event.gesture_type == 0x4c ) {
			ts_info("GTP gesture report single tap");
			key_value = KEY_GOTO;
		}
		input_report_key(cd->input_dev, key_value, 1);
		input_sync(cd->input_dev);
		input_report_key(cd->input_dev, key_value, 0);
		input_sync(cd->input_dev);
		goto re_send_ges_cmd;
	} else {
		ts_info("unsupported gesture:%x", gs_event.gesture_type);
	}

re_send_ges_cmd:
	if (hw_ops->gesture(cd, 0))
		ts_info("warning: failed re_send gesture cmd");
	if (!cd->tools_ctrl_sync)
		hw_ops->after_event_handler(cd);

	mutex_unlock(&cd->report_mutex);

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

	ret = hw_ops->gesture(cd, cd->gesture_enabled);
	if (ret)
		ts_err("failed enter gesture mode");
	else
		ts_info("enter gesture mode");

	hw_ops->irq_enable(cd, true);
	enable_irq_wake(cd->irq);

	return EVT_CANCEL_SUSPEND;
}

static int gsx_gesture_before_resume(struct goodix_ts_core *cd,
	struct goodix_ext_module *module)
{
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	hw_ops->irq_enable(cd, false);
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
	rwlock_init(&gsx_gesture->rwlock);

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

	goodix_gesture_enable(0);

	/* deinit sysfs */
	for (i = 0; i < ARRAY_SIZE(gesture_attrs); i++)
		sysfs_remove_file(&gsx_gesture->module.kobj,
					&gesture_attrs[i].attr);

	kobject_put(&gsx_gesture->module.kobj);
	kfree(gsx_gesture);
	module_initialized = false;
}
