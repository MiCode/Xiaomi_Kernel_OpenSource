/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include "inc/mtk_ir_core.h"
#include "inc/mtk_ir_common.h"

static enum MTK_IR_DEVICE_MODE g_ir_device_mode = MTK_IR_AS_IRRX;

static int x_small_step;
static int y_small_step;

static int x_large_step;
static int y_large_step;

int mtk_ir_mouse_get_x_smallstep(void)
{
	return x_small_step;
}

int mtk_ir_mouse_get_y_smallstep(void)
{
	return y_small_step;
}

int mtk_ir_mouse_get_x_largestep(void)
{
	return x_large_step;
}

int mtk_ir_mouse_get_y_largestep(void)
{
	return y_large_step;
}

void mtk_ir_mouse_set_x_smallstep(int xs)
{
	x_small_step = xs;
	MTK_IR_LOG("x_small_step = %d\n", x_small_step);
}

void mtk_ir_mouse_set_y_smallstep(int ys)
{
	y_small_step = ys;
	MTK_IR_LOG("y_small_step = %d\n", y_small_step);
}

void mtk_ir_mouse_set_x_largestep(int xl)
{
	x_large_step = xl;
	MTK_IR_LOG("x_large_step = %d\n", x_large_step);
}

void mtk_ir_mouse_set_y_largestep(int yl)
{
	y_large_step = yl;
	MTK_IR_LOG("y_large_step = %d\n", y_large_step);
}


enum MTK_IR_DEVICE_MODE mtk_ir_mouse_get_device_mode(void)
{
	return g_ir_device_mode;
}

void mtk_ir_mouse_set_device_mode(enum MTK_IR_DEVICE_MODE devmode)
{
	g_ir_device_mode = devmode;
	MTK_IR_LOG("g_ir_device_mode = %d\n", g_ir_device_mode);
}

int mtk_ir_mouse_proc_key(u32 scancode, struct mtk_ir_context *p_mtk_rc_core)
{
	struct input_dev *p_devmouse = p_mtk_rc_core->p_devmouse;
	struct mtk_ir_core_platform_data *pdata =
		p_mtk_rc_core->mtk_ir_ctl_data;
	struct rc_dev *rcdev = p_mtk_rc_core->rcdev;
	struct mtk_ir_mouse_code *p_mousecode = &(pdata->mouse_code);
	struct mtk_ir_context *cxt = NULL;

	ASSERT(p_devmouse != NULL);
	ASSERT(pdata != NULL);
	cxt = mtk_ir_context_obj;

	if (scancode == p_mousecode->scanleft) {
		if (!(rcdev->keypressed)) {
			MTK_IR_LOG("MOUSE X_LEFT PRESS\n");
			input_report_rel(p_devmouse, REL_X, -x_small_step);
			input_report_rel(p_devmouse, REL_Y, 0);
		} else {
			MTK_IR_LOG("MOUSE X_LEFT REPEAT\n");
			input_report_rel(p_devmouse, REL_X, -x_large_step);
			input_report_rel(p_devmouse, REL_Y, 0);
		}
	} else if (scancode == p_mousecode->scanright) {
		if (!(rcdev->keypressed)) {
			MTK_IR_LOG("MOUSE X_RIGHT PRESS\n");
			input_report_rel(p_devmouse, REL_X, x_small_step);
			input_report_rel(p_devmouse, REL_Y, 0);
		} else {
			MTK_IR_LOG("MOUSE X_RIGHT REPEAT\n");
			input_report_rel(p_devmouse, REL_X, x_large_step);
			input_report_rel(p_devmouse, REL_Y, 0);
		}
	} else if (scancode == p_mousecode->scanup) {
		if (!(rcdev->keypressed)) {
			MTK_IR_LOG("MOUSE Y_UP PRESS\n");
			input_report_rel(p_devmouse, REL_X, 0);
			input_report_rel(p_devmouse, REL_Y, -y_small_step);
		} else {
			MTK_IR_LOG("MOUSE Y_UP REPEAT\n");
			input_report_rel(p_devmouse, REL_X, 0);
			input_report_rel(p_devmouse, REL_Y, -y_large_step);
		}
	} else if (scancode == p_mousecode->scandown) {
		if (!(rcdev->keypressed)) {
			MTK_IR_LOG("MOUSE Y_DOWN PRESS\n");
			input_report_rel(p_devmouse, REL_X, 0);
			input_report_rel(p_devmouse, REL_Y, y_small_step);
		} else {
			MTK_IR_LOG("MOUSE Y_DOWN REPEAT\n");
			input_report_rel(p_devmouse, REL_X, 0);
			input_report_rel(p_devmouse, REL_Y, y_large_step);
		}
	} else if (scancode == p_mousecode->scanenter) {
		if (!(rcdev->keypressed)) {
			MTK_IR_LOG("MOUSE BTN_ENTER PRESS\n");
			input_report_key(p_devmouse, BTN_LEFT, 1);
			input_report_rel(p_devmouse, REL_X, 0);
			input_report_rel(p_devmouse, REL_Y, 0);
			input_sync(p_devmouse);

			MTK_IR_LOG("MOUSE BTN_ENTER RELEASE\n");
			input_report_key(p_devmouse, BTN_LEFT, 0);
			input_report_rel(p_devmouse, REL_X, 0);
			input_report_rel(p_devmouse, REL_Y, 0);
		}
	} else
		return 0;

	rc_keydown(rcdev, cxt->protocol, 0xffff, 0);
	input_sync(p_devmouse);
	return 1;
}

struct input_dev *mtk_ir_mouse_register_input(struct platform_device *pdev)
{
	struct input_dev *input_dev = NULL;
	struct mtk_ir_core_platform_data *pdata = get_mtk_ir_ctl_data();
	int ret = 0;

	ASSERT(pdev != NULL);
	x_small_step = pdata->mouse_step.x_step_s;
	x_large_step = pdata->mouse_step.x_step_l;
	y_small_step = pdata->mouse_step.y_step_s;
	y_large_step = pdata->mouse_step.y_step_l;

	input_dev = input_allocate_device();

	if (!input_dev) {
		MTK_IR_LOG("not enough memory for input device\n");
		goto end;
	}

	input_dev->name = pdata->mousename;
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.version = IR_VERSION + 1;
	input_dev->id.product = IR_PRODUCT + 1;
	input_dev->id.vendor = IR_VENDOR + 1;

	input_set_capability(input_dev, EV_REL, REL_X);
	input_set_capability(input_dev, EV_REL, REL_Y);
	input_set_capability(input_dev, EV_KEY, BTN_LEFT);
	input_set_capability(input_dev, EV_KEY, BTN_MIDDLE);
	input_set_capability(input_dev, EV_KEY, BTN_RIGHT);

	ret = input_register_device(input_dev);
	if (ret) {
		MTK_IR_LOG("could not register input device\n");
		goto fail_reg_dev;
	}

	MTK_IR_LOG("register input[%s] success\n", input_dev->name);
	goto end;

fail_reg_dev:
	input_free_device(input_dev);
	input_dev = NULL;
end:

	return input_dev;
}

void mtk_ir_mouse_unregister_input(struct input_dev *dev)
{
	ASSERT(dev != NULL);
	input_unregister_device(dev);
	MTK_IR_LOG("success\n");
}

