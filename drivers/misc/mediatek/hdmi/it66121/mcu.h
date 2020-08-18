/*
 * HDMI support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * HDMI TX driver for IT66121
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MCU_H_
#define _MCU_H_

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/workqueue.h>
/* #include <linux/earlysuspend.h> */
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/vmalloc.h>

/* #include <cust_eint.h> */
/* #include "cust_gpio_usage.h" */
/* #include "mach/eint.h" */
/* #include "mach/irqs.h" */

#if defined(CONFIG_ARCH_MT6575)
#include <mach/mt6575_devs.h>
#include <mach/mt6575_gpio.h>
#include <mach/mt6575_pm_ldo.h>
#include <mach/mt6575_typedefs.h>
#elif defined(CONFIG_ARCH_MT6577)
/* #include <mach/mt_typedefs.h> */
/* #include <mach/mt_gpio.h> */
#include <mach/mt_clock_manager.h>
#include <mach/mt_mdp.h>
#include <mach/mt_reg_base.h>
#endif

/* #define MySon */
/* #define Unixtar */

/***************************************/
/* DEBUG INFO define                   */
/**************************************/
/* #define Build_LIB */
/* #define MODE_RS232 */

/*************************************/
/*Port Using Define                  */
/*************************************/

/* #define _1PORT_ */

/************************************/
/* Function DEfine                  */
/***********************************/

/* #define       _HBR_I2S_ */

/* Include file */


/* Constant Definition */

#define TX0DEV 0
#define TX0ADR 0x98
#define TX0CECADR 0x9C

#define RXADR 0x90

#define EDID_ADR 0xA0 /* alex 070321 */

#define DELAY_TIME 1  /* unit=1 us; */
#define IDLE_TIME 100 /* unit=1 ms; */

#define HIGH 1
#define LOW 0

#define HPDON 1
#define HPDOFF 0

#endif /* _MCU_H_ */
