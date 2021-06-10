/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/device.h>
#include <dt-bindings/input/linux-event-codes.h>
#include "ddp_log.h"
#include "primary_display.h"

static atomic_t disp_tphint_trigger = ATOMIC_INIT(0);
static wait_queue_head_t disp_tphint_wait_queue;
static unsigned int disp_support_arr;

static void disp_tphint_event(struct input_handle *handle,
			unsigned int type, unsigned int code, int value)
{

}

static void disp_tphint_events(struct input_handle *handle,
			       const struct input_value *vals,
			       unsigned int count)
{
	if (disp_support_arr < 1) {
		return;
	}

	/* value 1: down, value 0: up */
	if (vals->type == EV_KEY
	    && vals->code == BTN_TOUCH
	    && vals->value == 1) {

		if (primary_display_current_fps(REQ_ARR_DFPS, false) >= 60 &&
		    primary_display_current_fps(HW_CURRENT_FPS, false) >= 60) {
			return;
		}
		atomic_set(&disp_tphint_trigger, 1);
		wake_up_interruptible(&disp_tphint_wait_queue);
		DISPMSG("disp_tphint_event, type %d, code %d, value %d!\n",
			vals->type, vals->code, vals->value);
	}
}

static int disp_tphint_connect(struct input_handler *handler,
			       struct input_dev *dev,
			       const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "disp_tphint";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	return 0;

 err_unregister_handle:
	input_unregister_handle(handle);
 err_free_handle:
	kfree(handle);
	return error;
}

static void disp_tphint_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id disp_tphint_ids[] = {
	{ .driver_info = 1 },	/* Matches all devices */
	{ },			/* Terminating zero entry */
};

MODULE_DEVICE_TABLE(input, disp_tphint_ids);

static struct input_handler disp_tphint_handler = {
	.event		= disp_tphint_event,
	.events		= disp_tphint_events,
	.connect	= disp_tphint_connect,
	.disconnect	= disp_tphint_disconnect,
	.name		= "disp_tphint",
	.id_table	= disp_tphint_ids,
};

int disp_tphint_wait_trigger(unsigned int *hint)
{
	int ret = 0;

	if (disp_support_arr < 1)
		DISP_PR_INFO("%s, ARR not enable\n", __func__);
	else
		DISPMSG("%s, trigger %d\n", __func__,
			atomic_read(&disp_tphint_trigger));

	/*  reset status and wait for touch hint*/
	atomic_set(&disp_tphint_trigger, 0);
	ret = wait_event_interruptible(disp_tphint_wait_queue,
		atomic_read(&disp_tphint_trigger));
	*hint = atomic_read(&disp_tphint_trigger);
	atomic_set(&disp_tphint_trigger, 0);
	if (ret < 0) {
		*hint = 0;
		DISP_PR_INFO("disp_tphint wait_event unexpect, ret:%d\n", ret);
		return ret;
	}
	DISPMSG("%s, hint:%d\n", __func__, *hint);

	return ret;
}

void disp_tphint_reset_status(void)
{
	disp_support_arr = primary_display_is_support_ARR();
}

static int __init disp_tphint_init(void)
{
	disp_tphint_reset_status();
	init_waitqueue_head(&disp_tphint_wait_queue);
	return input_register_handler(&disp_tphint_handler);
}

static void __exit disp_tphint_exit(void)
{
	input_unregister_handler(&disp_tphint_handler);
}

module_init(disp_tphint_init);
module_exit(disp_tphint_exit);
