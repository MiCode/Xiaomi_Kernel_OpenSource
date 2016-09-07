/* Copyright (c) 2012-2013, 2016 The Linux Foundation. All rights reserved.
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

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input/mt.h>
#include <linux/syscalls.h>
#include "usfcdev.h"

#define UNDEF_ID    0xffffffff
#define SLOT_CMD_ID 0
#define MAX_RETRIES 10

enum usdev_event_status {
	USFCDEV_EVENT_ENABLED,
	USFCDEV_EVENT_DISABLING,
	USFCDEV_EVENT_DISABLED,
};

struct usfcdev_event {
	bool (*match_cb)(uint16_t, struct input_dev *dev);
	bool registered_event;
	bool interleaved;
	enum usdev_event_status event_status;
};
static struct usfcdev_event s_usfcdev_events[MAX_EVENT_TYPE_NUM];

struct usfcdev_input_command {
	unsigned int type;
	unsigned int code;
	unsigned int value;
};

static long  s_usf_pid;

static bool usfcdev_filter(struct input_handle *handle,
			 unsigned int type, unsigned int code, int value);
static bool usfcdev_match(struct input_handler *handler,
				struct input_dev *dev);
static int usfcdev_connect(struct input_handler *handler,
				struct input_dev *dev,
				const struct input_device_id *id);
static void usfcdev_disconnect(struct input_handle *handle);

static const struct input_device_id usfc_tsc_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		/* assumption: ABS_X & ABS_Y are in the same long */
		.absbit = { [BIT_WORD(ABS_X)] = BIT_MASK(ABS_X) |
						BIT_MASK(ABS_Y) },
	},
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		/* assumption: MT_.._X & MT_.._Y are in the same long */
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) },
	},
	{ } /* Terminating entry */
};

MODULE_DEVICE_TABLE(input, usfc_tsc_ids);

static struct input_handler s_usfc_handlers[MAX_EVENT_TYPE_NUM] = {
	{ /* TSC handler */
		.filter         = usfcdev_filter,
		.match          = usfcdev_match,
		.connect        = usfcdev_connect,
		.disconnect     = usfcdev_disconnect,
		/* .minor can be used as index in the container, */
		/*  because .fops isn't supported */
		.minor          = TSC_EVENT_TYPE_IND,
		.name           = "usfc_tsc_handler",
		.id_table       = usfc_tsc_ids,
	},
};

/*
 * For each event type, there are a number conflicting devices (handles)
 * The first registered device (primary) is real TSC device; it's mandatory
 * Optionally, later registered devices are simulated ones.
 * They are dynamically managed
 * The primary device's handles are stored in the below static array
 */
static struct input_handle s_usfc_primary_handles[MAX_EVENT_TYPE_NUM] = {
	{ /* TSC handle */
		.handler	= &s_usfc_handlers[TSC_EVENT_TYPE_IND],
		.name		= "usfc_tsc_handle",
	},
};

static struct usfcdev_input_command initial_clear_cmds[] = {
	{EV_ABS, ABS_PRESSURE,               0},
	{EV_KEY, BTN_TOUCH,                  0},
};

static struct usfcdev_input_command slot_clear_cmds[] = {
	{EV_ABS, ABS_MT_SLOT,               0},
	{EV_ABS, ABS_MT_TRACKING_ID, UNDEF_ID},
};

static struct usfcdev_input_command no_filter_cmds[] = {
	{EV_ABS, ABS_MT_SLOT,               0},
	{EV_ABS, ABS_MT_TRACKING_ID, UNDEF_ID},
	{EV_SYN, SYN_REPORT,                0},
};

static bool usfcdev_match(struct input_handler *handler, struct input_dev *dev)
{
	bool rc = false;
	int ind = handler->minor;

	pr_debug("%s: name=[%s]; ind=%d\n", __func__, dev->name, ind);

	if (s_usfcdev_events[ind].registered_event &&
		s_usfcdev_events[ind].match_cb) {
		rc = (*s_usfcdev_events[ind].match_cb)((uint16_t)ind, dev);
		pr_debug("%s: [%s]; rc=%d\n", __func__, dev->name, rc);
	}
	return rc;
}

static int usfcdev_connect(struct input_handler *handler, struct input_dev *dev,
				const struct input_device_id *id)
{
	int ret = 0;
	uint16_t ind = handler->minor;
	struct input_handle *usfc_handle = NULL;

	if (s_usfc_primary_handles[ind].dev == NULL) {
		pr_debug("%s: primary device; ind=%d\n",
			__func__,
			ind);
		usfc_handle = &s_usfc_primary_handles[ind];
	} else {
		pr_debug("%s: secondary device; ind=%d\n",
			__func__,
			ind);
		usfc_handle = kzalloc(sizeof(struct input_handle),
					GFP_KERNEL);
		if (!usfc_handle) {
			pr_err("%s: memory allocation failed; ind=%d\n",
				__func__,
				ind);
			return -ENOMEM;
		}
		usfc_handle->handler = &s_usfc_handlers[ind];
		usfc_handle->name = s_usfc_primary_handles[ind].name;
	}
	usfc_handle->dev = dev;
	ret = input_register_handle(usfc_handle);
	pr_debug("%s: name=[%s]; ind=%d; dev=0x%pK\n",
		 __func__,
		dev->name,
		ind,
		usfc_handle->dev);
	if (ret)
		pr_err("%s: input_register_handle[%d] failed: ret=%d\n",
			__func__,
			ind,
			ret);
	else {
		ret = input_open_device(usfc_handle);
		if (ret) {
			pr_err("%s: input_open_device[%d] failed: ret=%d\n",
				__func__,
				ind,
				ret);
			input_unregister_handle(usfc_handle);
		} else
			pr_debug("%s: device[%d] is opened\n",
				__func__,
				ind);
	}

	return ret;
}

static void usfcdev_disconnect(struct input_handle *handle)
{
	int ind = handle->handler->minor;

	input_close_device(handle);
	input_unregister_handle(handle);
	pr_debug("%s: handle[%d], name=[%s] is disconnected\n",
		__func__,
		ind,
		handle->dev->name);
	if (s_usfc_primary_handles[ind].dev == handle->dev)
		s_usfc_primary_handles[ind].dev = NULL;
	else
		kfree(handle);
}

static bool usfcdev_filter(struct input_handle *handle,
			unsigned int type, unsigned int code, int value)
{
	uint16_t i = 0;
	uint16_t ind = (uint16_t)handle->handler->minor;
	bool rc = (s_usfcdev_events[ind].event_status != USFCDEV_EVENT_ENABLED);

	if (s_usf_pid == sys_getpid()) {
		/* Pass events from usfcdev driver */
		rc = false;
		pr_debug("%s: event_type=%d; type=%d; code=%d; val=%d",
			__func__,
			ind,
			type,
			code,
			value);
	} else if (s_usfcdev_events[ind].event_status ==
						USFCDEV_EVENT_DISABLING) {
		uint32_t u_value = value;
		s_usfcdev_events[ind].interleaved = true;
		/* Pass events for freeing slots from TSC driver */
		for (i = 0; i < ARRAY_SIZE(no_filter_cmds); ++i) {
			if ((no_filter_cmds[i].type == type) &&
			    (no_filter_cmds[i].code == code) &&
			    (no_filter_cmds[i].value <= u_value)) {
				rc = false;
				pr_debug("%s: no_filter_cmds[%d]; %d",
					__func__,
					i,
					no_filter_cmds[i].value);
				break;
			}
		}
	}

	return rc;
}

bool usfcdev_register(
	uint16_t event_type_ind,
	bool (*match_cb)(uint16_t, struct input_dev *dev))
{
	int ret = 0;
	bool rc = false;

	if ((event_type_ind >= MAX_EVENT_TYPE_NUM) || !match_cb) {
		pr_err("%s: wrong input: event_type_ind=%d; match_cb=0x%pK\n",
			__func__,
			event_type_ind,
			match_cb);
		return false;
	}

	if (s_usfcdev_events[event_type_ind].registered_event) {
		pr_info("%s: handler[%d] was already registered\n",
			__func__,
			event_type_ind);
		return true;
	}

	s_usfcdev_events[event_type_ind].registered_event = true;
	s_usfcdev_events[event_type_ind].match_cb = match_cb;
	s_usfcdev_events[event_type_ind].event_status = USFCDEV_EVENT_ENABLED;
	ret = input_register_handler(&s_usfc_handlers[event_type_ind]);
	if (!ret) {
		rc = true;
		pr_debug("%s: handler[%d] was registered\n",
			__func__,
			event_type_ind);
	} else {
		s_usfcdev_events[event_type_ind].registered_event = false;
		s_usfcdev_events[event_type_ind].match_cb = NULL;
		pr_err("%s: handler[%d] registration failed: ret=%d\n",
			__func__,
			event_type_ind,
			ret);
	}

	return rc;
}

void usfcdev_unregister(uint16_t event_type_ind)
{
	if (event_type_ind >= MAX_EVENT_TYPE_NUM) {
		pr_err("%s: wrong input: event_type_ind=%d\n",
			__func__,
			event_type_ind);
		return;
	}
	if (s_usfcdev_events[event_type_ind].registered_event) {
		input_unregister_handler(&s_usfc_handlers[event_type_ind]);
		pr_debug("%s: handler[%d] was unregistered\n",
			__func__,
			event_type_ind);
		s_usfcdev_events[event_type_ind].registered_event = false;
		s_usfcdev_events[event_type_ind].match_cb = NULL;
		s_usfcdev_events[event_type_ind].event_status =
							USFCDEV_EVENT_ENABLED;

	}
}

static inline void usfcdev_send_cmd(
	struct input_dev *dev,
	struct usfcdev_input_command cmd)
{
	input_event(dev, cmd.type, cmd.code, cmd.value);
}

static void usfcdev_clean_dev(uint16_t event_type_ind)
{
	struct input_dev *dev = NULL;
	int i;
	int j;
	int retries = 0;

	if (event_type_ind >= MAX_EVENT_TYPE_NUM) {
		pr_err("%s: wrong input: event_type_ind=%d\n",
			__func__,
			event_type_ind);
		return;
	}
	/* Only primary device must exist */
	dev = s_usfc_primary_handles[event_type_ind].dev;
	if (dev == NULL) {
		pr_err("%s: NULL primary device\n",
		__func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(initial_clear_cmds); i++)
		usfcdev_send_cmd(dev, initial_clear_cmds[i]);
	input_sync(dev);

	/* Send commands to free all slots */
	for (i = 0; i < dev->mt->num_slots; i++) {
		s_usfcdev_events[event_type_ind].interleaved = false;
		if (input_mt_get_value(&dev->mt->slots[i],
					ABS_MT_TRACKING_ID) < 0) {
			pr_debug("%s: skipping slot %d",
				__func__, i);
			continue;
		}
		slot_clear_cmds[SLOT_CMD_ID].value = i;
		for (j = 0; j < ARRAY_SIZE(slot_clear_cmds); j++)
			usfcdev_send_cmd(dev, slot_clear_cmds[j]);

		if (s_usfcdev_events[event_type_ind].interleaved) {
			pr_debug("%s: interleaved(%d): slot(%d)",
				__func__, i, dev->mt->slot);
			if (retries++ < MAX_RETRIES) {
				--i;
				continue;
			}
			pr_warn("%s: index(%d) reached max retires",
				__func__, i);
		}

		retries = 0;
		input_sync(dev);
	}
}

bool usfcdev_set_filter(uint16_t event_type_ind, bool filter)
{
	bool rc = true;

	if (event_type_ind >= MAX_EVENT_TYPE_NUM) {
		pr_err("%s: wrong input: event_type_ind=%d\n",
			__func__,
			event_type_ind);
		return false;
	}

	if (s_usfcdev_events[event_type_ind].registered_event) {

		pr_debug("%s: event_type[%d]; filter=%d\n",
			__func__,
			event_type_ind,
			filter
			);
		if (filter) {
			s_usfcdev_events[event_type_ind].event_status =
						USFCDEV_EVENT_DISABLING;
			s_usf_pid = sys_getpid();
			usfcdev_clean_dev(event_type_ind);
			s_usfcdev_events[event_type_ind].event_status =
						USFCDEV_EVENT_DISABLED;
		} else
			s_usfcdev_events[event_type_ind].event_status =
						USFCDEV_EVENT_ENABLED;
	} else {
		pr_err("%s: event_type[%d] isn't registered\n",
			__func__,
			event_type_ind);
		rc = false;
	}

	return rc;
}

static int __init usfcdev_init(void)
{
	return 0;
}

device_initcall(usfcdev_init);

MODULE_DESCRIPTION("Handle of events from devices, conflicting with USF");
