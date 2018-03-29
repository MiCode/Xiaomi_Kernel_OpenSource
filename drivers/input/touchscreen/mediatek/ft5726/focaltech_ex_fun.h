/************************************************************************
* File Name: focaltech_ex_fun.h
*
* Author:
*
* Created: 2015-01-01
*
* Abstract: function for fw upgrade, adb command, create apk second entrance
*
************************************************************************/
#ifndef __LINUX_fts_EX_FUN_H__
#define __LINUX_fts_EX_FUN_H__

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <asm/unistd.h>
#include <linux/uaccess.h>

/* #define AUTO_CLB */

/* FT5726 register address */
#define FTS_UPGRADE_AA		0xAA
#define FTS_UPGRADE_55		0x55
#define FTS_PACKET_LENGTH	128
#define FTS_UPGRADE_LOOP	3
#define UPGRADE_RETRY_LOOP	3
#define FTS_FACTORYMODE_VALUE	0x40
#define FTS_WORKMODE_VALUE	0x00

#define FTS_REG_CHIP_ID	0xA3
#define FTS_REG_FW_VER		0xA6
#define FTS_REG_POINT_RATE	0x88
#define FTS_REG_VENDOR_ID	0xA8
#define IC_INFO_DELAY_AA	20
#define IC_INFO_DELAY_55	20
#define IC_INFO_UPGRADE_ID1	0x58
#define IC_INFO_UPGRADE_ID2	0x2c

extern unsigned char hw_rev;
extern int apk_debug_flag;
extern struct tpd_device *tpd;
int hid_to_i2c(struct i2c_client *client);
int fts_ctpm_auto_upgrade(struct i2c_client *client);
int fts_create_sysfs(struct i2c_client *client);
int fts_create_apk_debug_channel(struct i2c_client *client);
void fts_release_apk_debug_channel(void);
void fts_release_sysfs(struct i2c_client *client);
#endif
