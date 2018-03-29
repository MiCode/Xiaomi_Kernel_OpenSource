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

#ifndef __LASTPC_H__
#define __LASTPC_H__

#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/compiler.h>

struct lastbus_plt;

struct lastbus_plt_operations {
	/* if platform needs special settings before */
	int (*start)(struct lastbus_plt *plt);
	/* dump anything we get */
	int (*dump)(struct lastbus_plt *plt, char *buf, int len);
	/* enable the lastbus functionality */
	int (*enable)(struct lastbus_plt *plt);
	/* if you want to add unit test by sysfs interface, implement this */
	int (*test)(struct lastbus_plt *plt, int test_case);
	/* if you want to show unit test by sysfs interface, implement this */
	int (*test_show)(char *buf);
	/* if you want to do anything more than lastbus.c:lastbus_probe() */
	int (*probe)(struct lastbus_plt *plt, struct platform_device *pdev);
	/* if you want to do anything more than lastbus.c:lastbus_remove() */
	int (*remove)(struct lastbus_plt *plt, struct platform_device *pdev);
	/* if you want to do anything more than lastbus.c:lastbus_suspend() */
	int (*suspend)(struct lastbus_plt *plt, struct platform_device *pdev, pm_message_t state);
	/* if you want to do anything more than lastbus.c:lastbus_resume() */
	int (*resume)(struct lastbus_plt *plt, struct platform_device *pdev);
};

struct lastbus_plt {
	unsigned int min_buf_len;
	struct lastbus_plt_operations *ops;
	struct lastbus *common;
};

struct lastbus {
	struct platform_driver plt_drv;
	void __iomem *mcu_base;
	void __iomem *peri_base;
	struct lastbus_plt *cur_plt;
};

/* for platform register their specific lastbus behaviors
   (chip or various versions of lastbus)
*/
int lastbus_register(struct lastbus_plt *plt);

#endif /* end of __LASTPC_H__ */
