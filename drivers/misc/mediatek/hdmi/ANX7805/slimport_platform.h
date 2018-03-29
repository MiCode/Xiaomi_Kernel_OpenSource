/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/
#include <linux/init.h>
	//#include <linux/string.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
	
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
	
#include <linux/semaphore.h>
#include <linux/mutex.h>
	/*#include <mach/mt_gpio.h>*/

#ifndef SLIMPORT_FEATURE
#define SLIMPORT_FEATURE
#endif


#define SP_TX_ANX7418_SLAVE_ADDR 0x50
#define SP_TX_ANX7418_OFFSET     0x36


/****************************************************************/
/*
#define SLIMPORT_TX_EDID_READ(context,fmt,arg...) 					\
	pr_debug(fmt, ## arg);
*/
#define SLIMPORT_TX_EDID_READ(context,fmt,arg...)

#define SLIMPORT_TX_EDID_INFO(context,fmt,arg...) 					\
	pr_info(fmt, ## arg);

#define SLIMPORT_TX_DBG_INFO(driver_context, fmt, arg...)				\
	pr_warn(fmt, ## arg);

#define SLIMPORT_TX_DBG_WARN(driver_context, fmt, arg...) 				\
	pr_warn(fmt, ## arg);

#define SLIMPORT_TX_DBG_ERR(driver_context, fmt, arg...)				\
	pr_err(fmt, ## arg);

typedef enum {
	RESET_PIN,
	PD_PIN,
	MAX_TYPE_PIN
} PIN_TYPE;
extern unsigned int mhl_eint_number;
extern unsigned int mhl_eint_gpio_number;

int HalOpenI2cDevice(char const *DeviceName, char const *DriverName);
void slimport_platform_init(void);
void register_slimport_eint(void);
void reset_mhl_board(int hwResetPeriod, int hwResetDelay, int is_power_on);
void i2s_gpio_ctrl(int enable);
void dpi_gpio_ctrl(int enable);
void set_pin_high_low(PIN_TYPE pin, bool is_high);

void Unmask_Slimport_Intr(void);
void Mask_Slimport_Intr(bool irq_context);

