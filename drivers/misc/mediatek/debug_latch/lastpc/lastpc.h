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

struct lastpc_plt;

struct lastpc_plt_operations {
	/* if platform needs special settings before */
	int (*start)(struct lastpc_plt *plt);
	/* dump anything we get */
	int (*dump)(struct lastpc_plt *plt, char *buf, int len);
	/* if you want to add unit test by sysfs interface, implement this */
	int (*reboot_test)(struct lastpc_plt *plt);
	/* if you want to do anything more than lastpc.c:lastpc_probe() */
	int (*probe)(struct lastpc_plt *plt, struct platform_device *pdev);
	/* if you want to do anything more than lastpc.c:lastpc_remove() */
	int (*remove)(struct lastpc_plt *plt, struct platform_device *pdev);
	/* if you want to do anything more than lastpc.c:lastpc_suspend() */
	int (*suspend)(struct lastpc_plt *plt, struct platform_device *pdev, pm_message_t state);
	/* if you want to do anything more than lastpc.c:lastpc_resume() */
	int (*resume)(struct lastpc_plt *plt, struct platform_device *pdev);
};

struct lastpc_plt {
	unsigned int chip_code;
	unsigned int min_buf_len;
	struct lastpc_plt_operations *ops;
	struct lastpc *common;
};

struct lastpc {
	struct platform_driver plt_drv;
	void __iomem *base;
	struct lastpc_plt *cur_plt;
};

/* for platform register their specific lastpc behaviors
   (chip or various versions of lastpc)
*/
int lastpc_register(struct lastpc_plt *plt);

#endif /* end of __LASTPC_H__ */
