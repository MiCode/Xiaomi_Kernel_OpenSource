/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef _ACCDET_H_
#define _ACCDET_H_

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

/* IOCTL */
#define ACCDET_DEVNAME "accdet"
#define ACCDET_IOC_MAGIC 'A'
#define ACCDET_INIT _IO(ACCDET_IOC_MAGIC, 0)
#define SET_CALL_STATE _IO(ACCDET_IOC_MAGIC, 1)
#define GET_BUTTON_STATUS _IO(ACCDET_IOC_MAGIC, 2)

/* define for phone call state */
#define CALL_IDLE 0
#define CALL_RINGING 1
#define CALL_ACTIVE 2
#define KEY_CALL	KEY_SEND
#define KEY_ENDCALL	KEY_HANGEUL

#define ACCDET_TIME_OUT 0x61A80	/*400us*/

extern s32 pwrap_read(u32 adr, u32 *rdata);
extern s32 pwrap_write(u32 adr, u32 wdata);
extern const struct file_operations *accdet_get_fops(void);/* from accdet_drv.c */
extern struct platform_driver accdet_driver_func(void);	/* from accdet_drv.c */
extern struct headset_mode_settings *get_cust_headset_settings(void);
extern struct headset_key_custom *get_headset_key_custom_setting(void);
extern void accdet_create_attr_func(void);	/* from accdet_drv.c */
extern const struct of_device_id accdet_of_match[];
void mt_accdet_remove(void);
void mt_accdet_suspend(void);
void mt_accdet_resume(void);
void mt_accdet_pm_restore_noirq(void);
long mt_accdet_unlocked_ioctl(unsigned int cmd, unsigned long arg);
int mt_accdet_probe(struct platform_device *dev);
int accdet_get_cable_type(void);
/* just be called by audio module */
extern int accdet_read_audio_res(unsigned int res_value);

/* globle ACCDET variables */
enum accdet_report_state {
	NO_DEVICE = 0,
	HEADSET_MIC = 1,/* 4pole*/
	HEADSET_NO_MIC = 2,/* 3pole */
	HEADSET_BI_MIC = 3,/* 5pole */
	LINE_OUT_DEVICE = 4,/* lineout */
};

enum accdet_status {
	PLUG_OUT = 0,
	MIC_BIAS = 1,/* 4pole*/
	HOOK_SWITCH = 2,/* 3pole*/
	BI_MIC_BIAS = 3,/* 5pole*/
	LINE_OUT = 4,/* lineout */
	STAND_BY = 5
};

enum hook_switch_result {
	DO_NOTHING = 0,
	ANSWER_CALL = 1,
	REJECT_CALL = 2
};

struct three_key_threshold {
	unsigned int mid_key;
	unsigned int up_key;
	unsigned int down_key;
};
struct four_key_threshold {
	unsigned int mid_key_four;/* function A: 70ohm less */
	unsigned int voice_key_four;/* function D: 110~180ohm */
	unsigned int up_key_four;/* function B: 210~290ohm */
	unsigned int down_key_four;/* function C: 360~680ohm */
};

/* default debounce8{0x800, 0x800, 0x800, 0x800, 0x0, 0x20}; */
struct config_accdet_param {
	unsigned int pwm_width;/* pwm frequence */
	unsigned int pwm_thresh;/* pwm duty */
	unsigned int fall_delay;/* falling stable time */
	unsigned int rise_delay;/* rising stable time */
	unsigned int debounce0;/* state00, 3pole | hook switch */
	unsigned int debounce1;/* state01, mic bias debounce */
	unsigned int debounce3;/* state11, plug out debounce */
	unsigned int debounce4;/* auxadc debounce */
};

struct config_headset_param {
	/* set mic bias voltage set: 0x02,1.9V;0x07,2.7V */
	unsigned int mic_bias_vol;
	/* set the plugout debounce */
	unsigned int accdet_plugout_deb;
	/* set mic bias mode:1,ACC;2,DCC,without internal bias;6,DCC,with internal bias */
	unsigned int accdet_mic_mode;
	/* eint0&eint1(same) level polarity,IRQ_TYPE_LEVEL_HIGH(4); */
	/* IRQ_TYPE_LEVEL_LOW(8);IRQ_TYPE_EDGE_FALLING(2);IRQ_TYPE_EDGE_RISING(1) */
	unsigned int eint_level_pol;
	/* set three key voltage threshold: 0--MD_MAX--UP_MAX--DW_MAX */
	struct three_key_threshold three_key;
	/* set three key voltage threshold: 0--MD_MAX--VOICE_MAX--UP_MAX--DW_MAX */
	struct four_key_threshold four_key;
	/* set accdet pwm & debounce time */
	struct config_accdet_param cfg_cust_accdet;
};

enum {
	accdet_state000 = 0,
	accdet_state001,
	accdet_state010,
	accdet_state011,
	accdet_state100,
	accdet_state101,
	accdet_state110,
	accdet_state111,
	accdet_auxadc
};


#ifdef CONFIG_ACCDET_EINT
extern struct platform_device accdet_device;
#endif
#endif

