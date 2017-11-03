/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _DSI_DISPLAY_TEST_H_
#define _DSI_DISPLAY_TEST_H_

#include "dsi_display.h"
#include "dsi_ctrl_hw.h"
#include "dsi_ctrl.h"

struct dsi_display_test {
	struct dsi_display *display;

	struct work_struct test_work;
};

int dsi_display_test_init(struct dsi_display *display);


#endif /* _DSI_DISPLAY_TEST_H_ */
