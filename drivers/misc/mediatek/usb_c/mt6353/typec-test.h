/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _TYPEC_TEST_H
#define _TYPEC_TEST_H

/*************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>

#include <asm/irq.h>
#include <asm/byteorder.h>

#include <linux/cdev.h>

#include "typec_mt6353.h"

/*************************************************************************/

/* CLI related */

#define CLI_MAGIC ('CLI')
#define IOCTL_READ _IOR(CLI_MAGIC, 0, int)
#define IOCTL_WRITE _IOW(CLI_MAGIC, 1, int)

#define CLI_BUF_SIZE 200
#define MAX_ARG_SIZE 20
#define MAX_CMD_SIZE 256

struct CMD_TBL_T {
	char name[MAX_CMD_SIZE];
	int (*cb_func)(struct file *file, int argc, char **argv);
};

/*************************************************************************/

int call_function(struct file *file, char *buf);

/*************************************************************************/

#endif /* _TYPEC_TEST_H */
