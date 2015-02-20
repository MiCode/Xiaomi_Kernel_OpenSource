/* -------------------------------------------------------------------------
 * Copyright (C) 2013-2014 Inside Secure
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * Copyright (C) 2014-2016, Intel Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * ------------------------------------------------------------------------- */

/*
 * FieldsPeak driver module.
 *
 */

#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/platform_data/fdp.h>

/* function prototypes */

int fdp_custom_init(void);
void fdp_custom_exit(void);
int fdp_custom_open(struct inode *inode, struct file *filp);
int fdp_custom_release(struct inode *inode, struct file *filp);
ssize_t fdp_custom_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos);
ssize_t fdp_custom_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *f_pos);
unsigned int fdp_custom_poll(struct file *filp, poll_table *wait);
long fdp_custom_ioctl_reset(struct file *filp, unsigned int cmd,
	unsigned long arg);
long fdp_custom_ioctl_clk_req(struct file *filp, unsigned int cmd,
	unsigned long arg);

/* EOF */
