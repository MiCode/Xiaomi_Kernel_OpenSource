/************************************************************************
* File Name: focaltech_ctl.c
*
* Author:
*
* Created: 2015-01-01
*
* Abstract: declare for IC info, Read/Write, reset
*
************************************************************************/
#ifndef TOUCHPANEL_H__
#define TOUCHPANEL_H__

#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/version.h>

/* IC info */
extern int ftxxxx_i2c_Read(struct i2c_client *client, char *writebuf,
				int writelen, char *readbuf, int readlen);
extern int ftxxxx_i2c_Write(struct i2c_client *client, char *writebuf,
				int writelen);
extern int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue);
extern int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue);
extern void fts_reset_tp(int HighOrLow);

/* Pre-defined definition */
#define TINNO_TOUCH_TRACK_IDS		10
#define CFG_MAX_TOUCH_POINTS		10
#define TPD_RES_X			1536
#define TPD_RES_Y			2048
#define IICReadWriteRetryTime		3

#endif /* TOUCHPANEL_H__ */
