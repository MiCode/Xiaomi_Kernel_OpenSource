// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, The Linux Foundation. All rights reserved.
 * Copyright (C) 2022 XiaoMi, Inc.
 */

#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "mtk_drm_trace.h"
#include "mi_disp_input_handler.h"
#include "mi_disp_print.h"
#include "mi_dsi_panel.h"

static uint64_t last_input_event_time = 0;

static void mi_disp_input_event_handler(struct input_handle *handle,
	unsigned int type, unsigned int code, int value)
{
	struct disp_display *dd_ptr = NULL;

	if (!handle || !handle->handler || !handle->handler->private) {
		DISP_ERROR("invalid dd_ptr for the input event\n");
		return;
	}

	dd_ptr = (struct disp_display *)handle->handler->private;
	if (!dd_ptr) {
		DISP_ERROR("invalid parameters\n");
		return;
	}

	last_input_event_time = local_clock();

	kthread_queue_work(&dd_ptr->d_thread.worker, &dd_ptr->input_event_work);
}

static int mi_disp_input_connect(struct input_handler *handler,
	struct input_dev *dev, const struct input_device_id *id)
{
	int rc = 0;
	struct input_handle *handle = kzalloc(sizeof(*handle), GFP_KERNEL);

	DISP_DEBUG("%s()\n", __func__);

	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = handler->name;

	rc = input_register_handle(handle);
	if (rc) {
		DISP_ERROR("failed to register input handle\n");
		goto error;
	}

	rc = input_open_device(handle);
	if (rc) {
		DISP_ERROR("failed to open input device\n");
		goto error_unregister;
	}

	return 0;

error_unregister:
	input_unregister_handle(handle);

error:
	kfree(handle);

	return rc;
}

static void mi_disp_input_disconnect(struct input_handle *handle)
{
	DISP_DEBUG("%s()\n", __func__);
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id mi_disp_input_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
					BIT_MASK(ABS_MT_POSITION_X) |
					BIT_MASK(ABS_MT_POSITION_Y) },
	},
	{ },
};

int mi_disp_input_handler_register(void *display, int disp_id, int intf_type)
{
	int rc = 0;
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;

	DISP_DEBUG("%s()\n", __func__);

	if (!df) {
		rc = -ENODEV;
		goto err;
	}

	dd_ptr = &df->d_display[disp_id];

	if (dd_ptr && dd_ptr->input_handler && !dd_ptr->input_handler->private) {
		dd_ptr->input_handler->private = dd_ptr;

		/* register input handler if not already registered */
		rc = input_register_handler(dd_ptr->input_handler);
		if (rc) {
			DISP_ERROR("input_handler_register failed, rc= %d\n", rc);
			kfree(dd_ptr->input_handler);
			dd_ptr->input_handler = NULL;
		}
	}
err:
	return rc;
}

int mi_disp_input_handler_unregister(void *display, int disp_id, int intf_type)
{
	int rc = 0;
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;

	DISP_DEBUG("%s()\n", __func__);

	if (!df) {
		rc = -ENODEV;
		goto err;
	}

	if (dd_ptr && dd_ptr->input_handler && dd_ptr->input_handler->private) {
		input_unregister_handler(dd_ptr->input_handler);
		dd_ptr->input_handler->private = NULL;
	}
err:
	return rc;
}

static void mi_disp_input_event_work_handler(struct kthread_work *work)
{
	struct mtk_dsi *dsi = NULL;
	struct disp_display *dd_ptr = container_of(work,
				struct disp_display, input_event_work);

	if (!dd_ptr || !dd_ptr->display) {
		DISP_ERROR("dd_ptr or dd_ptr->display is NULL\n");
		return;
	}

	dsi = (struct mtk_dsi *)dd_ptr->display;

	if (dd_ptr->intf_type == MI_INTF_DSI &&
			mtk_dsi_is_cmd_mode(&dsi->ddp_comp) &&
			mtk_drm_is_idle(dsi->encoder.crtc)) {
		DISP_DEBUG("%s: kicking idle\n", __func__);
		mtk_drm_trace_begin("kicking_idle");
		mtk_drm_idlemgr_kick(__func__, dsi->encoder.crtc, 1);
		mtk_drm_trace_end();
	}
}

int mi_disp_input_handler_init(struct disp_display *dd_ptr, int disp_id)
{
	struct input_handler *input_handler = NULL;
	int rc = 0;

	DISP_DEBUG("%s()\n", __func__);
	if (!dd_ptr || dd_ptr->input_handler) {
		DISP_ERROR("dd_ptr is NULL or input_handle is active\n");
		return -EINVAL;
	}

	input_handler = kzalloc(sizeof(*dd_ptr->input_handler), GFP_KERNEL);
	if (!input_handler)
		return -ENOMEM;

	input_handler->event = mi_disp_input_event_handler;
	input_handler->connect = mi_disp_input_connect;
	input_handler->disconnect = mi_disp_input_disconnect;
	input_handler->name = "mi_disp";
	input_handler->id_table = mi_disp_input_ids;

	dd_ptr->input_handler = input_handler;

	kthread_init_work(&dd_ptr->input_event_work,
			mi_disp_input_event_work_handler);
	return rc;
}

int mi_disp_input_handler_deinit(struct disp_display *dd_ptr, int disp_id)
{
	DISP_DEBUG("%s()\n", __func__);
	if (dd_ptr && dd_ptr->input_handler) {
		kfree(dd_ptr->input_handler);
		dd_ptr->input_handler = NULL;
	}
	return 0;
}

bool mi_disp_input_is_touch_active(void)
{
	return (local_clock() - last_input_event_time) < 100000000;
}
