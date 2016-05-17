/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/slab.h>

#include "dsi_display_test.h"

static void dsi_display_test_dump_modes(struct dsi_display_mode *mode, u32
					count)
{
}

static void dsi_display_test_work(struct work_struct *work)
{
	struct dsi_display_test *test;
	struct dsi_display *display;
	struct dsi_display_mode *modes;
	u32 count = 0;
	u32 size = 0;
	int rc = 0;

	test = container_of(work, struct dsi_display_test, test_work);

	display = test->display;
	rc = dsi_display_get_modes(display, NULL, &count);
	if (rc) {
		pr_err("failed to get modes count, rc=%d\n", rc);
		goto test_fail;
	}

	size = count * sizeof(*modes);
	modes = kzalloc(size, GFP_KERNEL);
	if (!modes) {
		rc = -ENOMEM;
		goto test_fail;
	}

	rc = dsi_display_get_modes(display, modes, &count);
	if (rc) {
		pr_err("failed to get modes, rc=%d\n", rc);
		goto test_fail_free_modes;
	}

	dsi_display_test_dump_modes(modes, count);

	rc = dsi_display_set_mode(display, &modes[0], 0x0);
	if (rc) {
		pr_err("failed to set mode, rc=%d\n", rc);
		goto test_fail_free_modes;
	}

	rc = dsi_display_prepare(display);
	if (rc) {
		pr_err("failed to prepare display, rc=%d\n", rc);
		goto test_fail_free_modes;
	}

	rc = dsi_display_enable(display);
	if (rc) {
		pr_err("failed to enable display, rc=%d\n", rc);
		goto test_fail_unprep_disp;
	}
	return;

test_fail_unprep_disp:
	if (rc) {
		pr_err("failed to unprep display, rc=%d\n", rc);
		goto test_fail_free_modes;
	}

test_fail_free_modes:
	kfree(modes);
test_fail:
	return;
}

int dsi_display_test_init(struct dsi_display *display)
{
	static int done;
	int rc = 0;
	struct dsi_display_test *test;

	if (done)
		return rc;

	done = 1;
	if (!display) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	test = kzalloc(sizeof(*test), GFP_KERNEL);
	if (!test)
		return -ENOMEM;

	test->display = display;
	INIT_WORK(&test->test_work, dsi_display_test_work);

	dsi_display_test_work(&test->test_work);
	return rc;
}

