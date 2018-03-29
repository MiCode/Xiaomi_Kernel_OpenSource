/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/
#ifndef _MT_GPIO_CORE_H_
#define _MT_GPIO_CORE_H_
/*
#if (defined(CONFIG_FPGA))
//#define CONFIG_MT_GPIO_FPGA_ENABLE
#include <mach/mt_gpio_fpga.h>
#else
// FIX-ME: marked for early porting
#include <cust_gpio_usage.h>
#include <mach/mt_gpio_base.h>
#endif
#include <mach/mt_gpio_ext.h>
*/
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif
/******************************************************************************
 MACRO Definition
******************************************************************************/
#define GPIO_DEVICE "mt-gpio"
#define VERSION     GPIO_DEVICE

#define GPIOTAG                "[GPIO] "
#define GPIOLOG(fmt, arg...)   pr_debug(GPIOTAG fmt, ##arg)
#define GPIOMSG(fmt, arg...)   pr_warn(fmt, ##arg)
#define GPIOERR(fmt, arg...)   pr_err(GPIOTAG "%5d: "fmt, __LINE__, ##arg)
#define GPIOFUC(fmt, arg...)	/* printk(GPIOTAG "%s\n", __FUNCTION__) */
/*----------------------------------------------------------------------------*/
/* Error Code No. */
#define RSUCCESS        0
#define ERACCESS        1
#define ERINVAL         2
#define ERWRAPPER		3

#define GPIO_RETERR(res, fmt, args...)                                               \
do {                                                                             \
		GPIOERR("%s:%04d: " fmt"\n", __func__, __LINE__, ##args);\
		return res;                                                                  \
} while (0)
#define GIO_INVALID_OBJ(ptr)   ((ptr) != mt_gpio)

enum {
	MT_BASE = 0,
	MT_EXT,
	MT_NOT_SUPPORT,
};
#if defined CONFIG_FPGA_EARLY_PORTING


#define MT_GPIO_PLACE(pin) ({\
	int ret = -1;\
	if ((pin >= 0) && (pin < 200)) {\
		ret = -1;\
		GPIOFUC("pin in base is %d\n", (int)pin);\
	} else if ((pin >= 0) && (pin < 200)) {\
		ret = -1;\
		GPIOFUC("pin in ext is %d\n", (int)pin);\
	} else{\
		GPIOERR("Pin number error %d\n", (int)pin);	\
		ret = -1;\
	} \
	ret; })


#else
#define MT_GPIO_PLACE(pin) ({\
	int ret = -1;\
	if ((pin >= MT_GPIO_BASE_START) && (pin < MT_GPIO_BASE_MAX)) {\
		ret = MT_BASE;\
		GPIOFUC("pin in base is %d\n", (int)pin);\
	} else if ((pin >= MT_GPIO_EXT_START) && (pin < MT_GPIO_EXT_MAX)) {\
		ret = MT_EXT;\
		GPIOFUC("pin in ext is %d\n", (int)pin);\
	} else{\
		GPIOERR("Pin number error %d\n", (int)pin);	\
		ret = -1;\
	} \
	ret; })

#endif
/* int where_is(unsigned long pin) */
/* { */
/* int ret = -1; */
/* //    GPIOLOG("pin is %d\n",pin); */
/* if((pin >= MT_GPIO_BASE_START) && (pin < MT_GPIO_BASE_MAX)){ */
/* ret = MT_BASE; */
/* //GPIOLOG("pin in base is %d\n",pin); */
/* }else if((pin >= MT_GPIO_EXT_START) && (pin < MT_GPIO_EXT_MAX)){ */
/* ret = MT_EXT; */
/* //GPIOLOG("pin in ext is %d\n",pin); */
/* }else{ */
/* GPIOERR("Pin number error %d\n",pin); */
/* ret = -1; */
/* } */
/* return ret; */
/* } */
/*decrypt pin*/

/*---------------------------------------------------------------------------*/
int mt_set_gpio_dir_base(unsigned long pin, unsigned long dir);
int mt_get_gpio_dir_base(unsigned long pin);
int mt_set_gpio_pull_enable_base(unsigned long pin, unsigned long enable);
int mt_get_gpio_pull_enable_base(unsigned long pin);
int mt_set_gpio_smt_base(unsigned long pin, unsigned long enable);
int mt_get_gpio_smt_base(unsigned long pin);
int mt_set_gpio_ies_base(unsigned long pin, unsigned long enable);
int mt_get_gpio_ies_base(unsigned long pin);
int mt_set_gpio_slew_rate_base(unsigned long pin, unsigned long enable);
int mt_get_gpio_slew_rate_base(unsigned long pin);
int mt_set_gpio_pull_select_base(unsigned long pin, unsigned long select);
int mt_get_gpio_pull_select_base(unsigned long pin);
int mt_set_gpio_pull_resistor_base(unsigned long pin, unsigned long resistors);
int mt_get_gpio_pull_resistor_base(unsigned long pin);
int mt_set_gpio_inversion_base(unsigned long pin, unsigned long enable);
int mt_get_gpio_inversion_base(unsigned long pin);
int mt_set_gpio_out_base(unsigned long pin, unsigned long output);
int mt_get_gpio_out_base(unsigned long pin);
int mt_get_gpio_in_base(unsigned long pin);
int mt_set_gpio_mode_base(unsigned long pin, unsigned long mode);
int mt_get_gpio_mode_base(unsigned long pin);
int mt_set_gpio_driving_base(unsigned long pin, unsigned long strength);
int mt_get_gpio_driving_base(unsigned long pin);
#ifdef CONFIG_PM
void mt_gpio_suspend(void);
void mt_gpio_resume(void);
#endif				/*CONFIG_PM */
#ifdef CONFIG_OF
void get_gpio_vbase(struct device_node *node);
void get_io_cfg_vbase(void);
#endif
#ifdef CONFIG_MD32_SUPPORT
void md32_gpio_handle_init(void);
#endif

int mt_set_gpio_dir_ext(unsigned long pin, unsigned long dir);
int mt_get_gpio_dir_ext(unsigned long pin);
int mt_set_gpio_pull_enable_ext(unsigned long pin, unsigned long enable);
int mt_get_gpio_pull_enable_ext(unsigned long pin);
int mt_set_gpio_smt_ext(unsigned long pin, unsigned long enable);
int mt_get_gpio_smt_ext(unsigned long pin);
int mt_set_gpio_ies_ext(unsigned long pin, unsigned long enable);
int mt_get_gpio_ies_ext(unsigned long pin);
int mt_set_gpio_pull_select_ext(unsigned long pin, unsigned long select);
int mt_get_gpio_pull_select_ext(unsigned long pin);
int mt_set_gpio_inversion_ext(unsigned long pin, unsigned long enable);
int mt_get_gpio_inversion_ext(unsigned long pin);
int mt_set_gpio_out_ext(unsigned long pin, unsigned long output);
int mt_get_gpio_out_ext(unsigned long pin);
int mt_get_gpio_in_ext(unsigned long pin);
int mt_set_gpio_mode_ext(unsigned long pin, unsigned long mode);
int mt_get_gpio_mode_ext(unsigned long pin);

void mt_gpio_pin_decrypt(unsigned long *cipher);
int mt_gpio_create_attr(struct device *dev);
int mt_gpio_delete_attr(struct device *dev);

#endif
