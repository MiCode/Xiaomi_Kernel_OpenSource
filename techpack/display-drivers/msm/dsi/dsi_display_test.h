/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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
