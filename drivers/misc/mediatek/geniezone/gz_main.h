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

static int __init gz_init(void);
static void __exit gz_exit(void);

static int gz_dev_open(struct inode *inode, struct file *filp);
static int gz_dev_release(struct inode *inode, struct file *filp);
static ssize_t gz_dev_read(struct file *filp, char __user *buf, size_t count,
			   loff_t *pos);
static ssize_t gz_dev_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *pos);

static long gz_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
static long gz_compat_ioctl(struct file *filep, unsigned int cmd,
			    unsigned long arg);
