/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include "usfcdev.h"

struct usfcdev_event {
	bool (*match_cb)(uint16_t, struct input_dev *dev);
	bool registered_event;
	bool filter;
};
static struct usfcdev_event s_usfcdev_events[MAX_EVENT_TYPE_NUM];

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
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
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

/* For each event type, one conflicting device (and handle) is supported */
static struct input_handle s_usfc_handles[MAX_EVENT_TYPE_NUM] = {
	{ /* TSC handle */
		.handler	= &s_usfc_handlers[TSC_EVENT_TYPE_IND],
		.name		= "usfc_tsc_handle",
	},
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

	s_usfc_handles[ind].dev = dev;
	ret = input_register_handle(&s_usfc_handles[ind]);
	if (ret) {
		pr_err("%s: input_register_handle[%d] failed: ret=%d\n",
			__func__,
			ind,
			ret);
	} else {
		ret = input_open_device(&s_usfc_handles[ind]);
		if (ret) {
			pr_err("%s: input_open_device[%d] failed: ret=%d\n",
				__func__,
				ind,
				ret);
			input_unregister_handle(&s_usfc_handles[ind]);
		} else
			pr_debug("%s: device[%d] is opened\n",
				__func__,
				ind);
	}

	return ret;
}

static void usfcdev_disconnect(struct input_handle *handle)
{
	input_unregister_handle(handle);
	pr_debug("%s: handle[%d] is disconnect\n",
		__func__,
		handle->handler->minor);
}

static bool usfcdev_filter(struct input_handle *handle,
			unsigned int type, unsigned int code, int value)
{
	uint16_t ind = (uint16_t)handle->handler->minor;

	pr_debug("%s: event_type=%d; filter=%d\n",
		__func__,
		ind,
		s_usfcdev_events[ind].filter);

	return s_usfcdev_events[ind].filter;
}

bool usfcdev_register(
	uint16_t event_type_ind,
	bool (*match_cb)(uint16_t, struct input_dev *dev))
{
	int ret = 0;
	bool rc = false;

	if ((event_type_ind >= MAX_EVENT_TYPE_NUM) || !match_cb) {
		pr_err("%s: wrong input: event_type_ind=%d; match_cb=0x%p\n",
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
	s_usfcdev_events[event_type_ind].filter = false;
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
		s_usfcdev_events[event_type_ind].filter = false;
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
		s_usfcdev_events[event_type_ind].filter = filter;
		pr_debug("%s: event_type[%d]; filter=%d\n",
			__func__,
			event_type_ind,
			filter
			);
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
