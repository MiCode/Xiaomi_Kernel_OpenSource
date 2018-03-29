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

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <uapi/asm-generic/fcntl.h>
#include <linux/err.h>
#include "ipanic.h"


struct file *expdb_open(void)
{
	static struct file *filp_expdb;

	if (!filp_expdb)
		filp_expdb = filp_open(AEE_EXPDB_PATH, O_RDWR, 0);
	if (IS_ERR(filp_expdb))
		LOGD("filp_open(%s) for aee failed (%ld)\n", AEE_EXPDB_PATH, PTR_ERR(filp_expdb));
	return filp_expdb;
}

ssize_t expdb_write(struct file *filp, const char *buf, size_t len, loff_t off)
{
	return kernel_write(filp, buf, len, off);
}

ssize_t expdb_read(struct file *filp, char *buf, size_t len, loff_t off)
{
	return kernel_read(filp, off, buf, len);
}

char *expdb_read_size(int off, int len)
{
	int ret;
	struct file *filp;
	char *data;
	int timeout = 0;

	do {
		filp = expdb_open();
		if (timeout++ > 3) {
			LOGE("open expdb partition fail [%ld]!\n", PTR_ERR(filp));
			return NULL;
		}
		msleep(500);
	} while (IS_ERR(filp));
	data = kzalloc(len, GFP_KERNEL);
	ret = kernel_read(filp, off, data, len);
	fput(filp);
	if (IS_ERR(ERR_PTR(ret))) {
		kfree(data);
		data = NULL;
		LOGE("read from expdb fail [%d]!\n", ret);
	}
	return data;
}
