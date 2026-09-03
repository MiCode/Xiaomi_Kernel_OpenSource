// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include "sysfs_display.h"

struct class *display_class;

int sprd_display_class_init(void)
{
	pr_info("display class register\n");

	display_class = class_create(THIS_MODULE, "display");
	if (IS_ERR(display_class)) {
		pr_err("Unable to create display class\n");
		return PTR_ERR(display_class);
	}

	return 0;
}

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_AUTHOR("Kevin Tang <kevin.tang@unisoc.com>");
MODULE_DESCRIPTION("Provide display class for hardware driver");
MODULE_LICENSE("GPL v2");
