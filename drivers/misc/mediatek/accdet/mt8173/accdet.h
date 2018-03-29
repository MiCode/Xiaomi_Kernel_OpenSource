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

#ifndef _ACCDEH_H_
#define _ACCDEH_H_
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>

#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/switch.h>
#include <linux/delay.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <linux/time.h>

#include <linux/string.h>
#include "reg_accdet.h"

/*----------------------------------------------------------------------
IOCTL
----------------------------------------------------------------------*/
#define ACCDET_DEVNAME "accdet"
#define ACCDET_IOC_MAGIC 'A'
#define ACCDET_INIT _IO(ACCDET_IOC_MAGIC, 0)
#define SET_CALL_STATE _IO(ACCDET_IOC_MAGIC, 1)
#define GET_BUTTON_STATUS _IO(ACCDET_IOC_MAGIC, 2)

/*define for phone call state*/

#define CALL_IDLE 0
#define CALL_RINGING 1
#define CALL_ACTIVE 2
#define KEY_CALL	KEY_SEND
#define KEY_ENDCALL	KEY_HANGEUL

#define ACCDET_TIME_OUT 0x61A80	/*400us*/
extern s32 pwrap_read(u32 adr, u32 *rdata);
extern s32 pwrap_write(u32 adr, u32 wdata);
extern const struct file_operations *accdet_get_fops(void);/*from accdet_drv.c*/
extern struct platform_driver accdet_driver_func(void);	/*from accdet_drv.c*/
extern struct headset_mode_settings *get_cust_headset_settings(void);
extern struct headset_key_custom *get_headset_key_custom_setting(void);
extern void accdet_create_attr_func(void);	/*from accdet_drv.c*/
#if defined(ACCDET_TS3A225E_PIN_SWAP)
extern int ts3a225e_read_byte(unsigned char cmd, unsigned char *returnData);
extern int ts3a225e_write_byte(unsigned char cmd, unsigned char writeData);
#endif
extern struct of_device_id accdet_of_match[];
void mt_accdet_remove(void);
void mt_accdet_suspend(void);
void mt_accdet_resume(void);
void mt_accdet_pm_restore_noirq(void);
long mt_accdet_unlocked_ioctl(unsigned int cmd, unsigned long arg);
int mt_accdet_probe(struct platform_device *dev);
int accdet_get_cable_type(void);

/****************************************************
globle ACCDET variables
****************************************************/

enum accdet_report_state {
	NO_DEVICE = 0,
	HEADSET_MIC = 1,
	HEADSET_NO_MIC = 2,
	/*HEADSET_ILEGAL = 3,*/
	/*DOUBLE_CHECK_TV = 4*/
};

enum accdet_status {
	PLUG_OUT = 0,
	MIC_BIAS = 1,
	/*DOUBLE_CHECK = 2,*/
	HOOK_SWITCH = 2,
	/*MIC_BIAS_ILLEGAL =3,*/
	/*TV_OUT = 5,*/
	STAND_BY = 4
};


enum hook_switch_result {
	DO_NOTHING = 0,
	ANSWER_CALL = 1,
	REJECT_CALL = 2
};
struct headset_mode_settings {
	int pwm_width;		/*pwm frequence*/
	int pwm_thresh;		/*pwm duty*/
	int fall_delay;		/*falling stable time*/
	int rise_delay;		/*rising stable time*/
	int debounce0;		/*hook switch or double check debounce*/
	int debounce1;		/*mic bias debounce*/
	int debounce3;		/*plug out debounce*/
};
struct three_key_threshold {
	int mid_key;
	int up_key;
	int down_key;
};
struct four_key_threshold {
	int mid_key_four;
	int voice_key_four;
	int up_key_four;
	int down_key_four;
};
struct head_dts_data {
	int mic_mode_vol;
	struct headset_mode_settings headset_debounce;
	int accdet_plugout_debounce;
	int accdet_mic_mode;
	struct three_key_threshold three_key;
	struct four_key_threshold four_key;
};
#ifdef CONFIG_ACCDET_EINT
extern struct platform_device accdet_device;
#endif
#endif
