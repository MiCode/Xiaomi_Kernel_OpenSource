/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
*This program is free software; you can redistribute it and/or
*modify it under the terms of the GNU General Public License as
*published by the Free Software Foundation version 2.
*This program is distributed AS-IS WITHOUT ANY WARRANTY of any
*kind, whether express or implied; INCLUDING without the implied warranty
*of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
*See the GNU General Public License for more details at
*http://www.gnu.org/licenses/gpl-2.0.html.
*/
#ifndef _WRAP_H
#define _WRAP_H
#include <linux/module.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/stringify.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/export.h>
#include <linux/kthread.h>
#include <linux/irqreturn.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <asm-generic/bitops/non-atomic.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/wakelock.h>

#define pr_cmd pr_debug

#define TASK_RET_TYPE void
#define TASK_ARG_TYPE  WORK_STRUCT
#define WORK_STRUCT struct work_struct
#define WAIT_QUEUE_HEAD_T wait_queue_head_t
#define WORK_QUEUE_STRUCT struct workqueue_struct

typedef TASK_RET_TYPE(*FUNCPtr) (TASK_ARG_TYPE *);

#define ANSI_ESC_RESET_TEXT "\x1b[0m"
#define ANSI_ESC_YELLOW_BG "\x1b[43m"
#define ANSI_ESC_WHITE_BG "\x1b[47m"
#define ANSI_ESC_RED_TEXT "\x1b[31m"
#define ANSI_ESC_YELLOW_TEXT "\x1b[33m"
#define ANSI_ESC_GREEN_TEXT "\x1b[32m"
#define ANSI_ESC_BLACK_TEXT "\1b[30m"
#define ANSI_ESC_WHITE_TEXT "\x1b[37m\x1b[1m"
#define ANSI_ESC_MAGENTA_TEXT "\x1b[35m"
#define ANSI_ESC_CYAN_TEXT "\x1b[36m"

/* Timers related */
struct usbpd_timer {
	void *timerhandle;
	uint32_t time_msec;
	bool periodicity;
};

/* Sabre Specific */
#include "../Common/si_usbpd_regs.h"

/* CONFIGURATION RELATED */
#define SABRE_DFP

#define I2C_DBG_SYSFS

#define GPIO_USBPD_INT	9
#define GPIO_VBUS_SRC	85
#define GPIO_VBUS_SNK	78
#define GPIO_RESET_CTRL	64

struct list_node {
	uint8_t data[28];
	uint16_t header;
	struct list_node *link;
};

struct list_node *list_node_create(void);

#define TASK_STRUCT struct task_struct*
#define WAIT_QUEUE_HEAD_T wait_queue_head_t
/* I2C Wrappers Related */
#define SII_DRV_DEVICE_I2C_ADDR 0x68
#define I2C_RETRY_MAX           10

void sii_i2c_init(struct i2c_adapter *adapter);
int sii_write_i2c_block(uint8_t offset, uint16_t count, const uint8_t *values);
int sii_read_i2c_block(uint8_t offset, uint16_t count, uint8_t *values);
/* Sabre Specific */
#define SII_DRIVER_NAME "sii70xx"
#define COMPATIBLE_NAME "simg,sii70xx"
#endif
