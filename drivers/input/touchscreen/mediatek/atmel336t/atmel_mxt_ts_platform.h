/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_ATMEL_MXT_TS_PLATFORM_H
#define __LINUX_ATMEL_MXT_TS_PLATFORM_H

#include <linux/types.h>

#define CONFIG_MXT_PROBE_ALTERNATIVE_CHIP 8
#define CONFIG_MXT_POWER_CONTROL_SUPPORT_AT_PROBE  //sunwf 
//#define CONFIG_MXT_T38_SKIP_LEN_AT_UPDATING 2
#define CONFIG_MXT_VENDOR_ID_BY_T19
#define CONFIG_MXT_FORCE_RESET_AT_POWERUP

//#define CONFIG_MXT_SELFCAP_TUNE

#define CONFIG_MXT_PLATFORM_MTK   /********************sunwf open about power*******************/
//#define CONFIG_MXT_PLATFORM_QUALCOMM
#if defined(CONFIG_MXT_PLATFORM_MTK)
#define CONFIG_MXT_IRQ_WORKQUEUE
#define CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE
#define CONFIG_MXT_EXTERNAL_MODULE
#define CONFIG_MXT_I2C_DMA
#define CONFIG_MXT_I2C_EXTFLAG
#else
#define CONFIG_FB_PM
#define CONFIG_MXT_IRQ_NESTED
#endif

#if defined(CONFIG_MXT_PLATFORM_MTK)
#include <mach/mt_pm_ldo.h>
#include <cust_eint.h>
#include "cust_gpio_usage.h"
#include <mach/mt_gpio.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include <mach/eint.h>
#include <mach/mt_pm_ldo.h>
#include "tpd.h"
#endif

#if defined(CONFIG_MXT_PLATFORM_QUALCOMM)
#include <linux/io.h>
#include <mach/gpio.h>
#endif

#if defined(CONFIG_MXT_EXTERNAL_MODULE)
#if !defined(TPD_DEVICE)
#include "atmel_mxt_ts_mtk_dummy.h"
#endif
#endif


#if defined(CONFIG_MXT_I2C_DMA)
#include <linux/dma-mapping.h>
static int __mxt_read(struct i2c_client *client,
			void *val, u16 len);
static int __mxt_write(struct i2c_client *client,
			const void *val, u16 len);
#endif

#if defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE) || defined(CONFIG_MXT_REPORT_VIRTUAL_KEY_SLOT_NUM)
static struct mxt_data *mxt_g_data;
#endif

#if defined(CONFIG_MXT_PLATFORM_MTK)
static void board_pulse_irq_thread(void);
#endif

static inline void board_gpio_init(const struct mxt_platform_data *pdata)
{

	// if gpio init in board, or use regulator , skip this function

	/* set irq pin input/pull high */
	/*	 power off vdd/avdd   */
	/*		msleep(100);		*/
	/* 	  set reset output 0 	*/
	/*	 power up vdd/avdd	*/
	/*		msleep(50);			*/
	/*	   set reset output 1 	*/
	/*		msleep(200);		*/

#if defined(CONFIG_MXT_PLATFORM_MTK)
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	msleep(10);
	
	TPD_DEBUG("*****sunwf111111 func = %s line = %d ******\n",__func__,__LINE__);
	
	hwPowerOn(MT6331_POWER_LDO_VGP1, VOL_3300, "TP"); //MT6323_POWER_LDO_VGP1/************sunwf modify about power************/
//	hwPowerOn(MT6331_POWER_LDO_VGP3, VOL_1800, "TP"); //MT6323_POWER_LDO_VGP1/************sunwf modify about power******************/
	msleep(50);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(250);
#endif
}
static inline void board_gpio_deinit(const struct mxt_platform_data *pdata)
{
#if defined(CONFIG_MXT_PLATFORM_QUALCOMM)
	MXT_GPIO_FREE(MXT_INT_PORT);
	MXT_GPIO_FREE(MXT_RST_PORT);
#endif
}
static void board_init_irq(const struct mxt_platform_data *pdata)
{
//Here should math irqflags in interface file
/*  
	1 IRQF_TRIGGER_FALLING: 
		<a>should set auto_unmask bit.
		<b>.irqflags = IRQF_TRIGGER_FALLING
	
	2 IRQF_TRIGGER_LOW: 
		<a>shouldn't set auto_unmask bit
		<b>.irqflags = IRQF_TRIGGER_LOW
*/
#if defined(CONFIG_MXT_PLATFORM_MTK)
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, /*CUST_EINTF_TRIGGER_FALLING*/EINTF_TRIGGER_LOW, board_pulse_irq_thread, 0); 
#endif
}

static inline void board_enable_irq(const struct mxt_platform_data *pdata, unsigned int irq)
{
#if defined(CONFIG_MXT_PLATFORM_MTK)
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#else
	enable_irq(irq);
#endif
}

static inline void board_enable_irq_wake(const struct mxt_platform_data *pdata, unsigned int irq)
{
#if defined(CONFIG_MXT_PLATFORM_MTK)
#else
	enable_irq_wake(irq);
#endif
}

static inline void board_disable_irq_wake(const struct mxt_platform_data *pdata, unsigned int irq)
{
#if defined(CONFIG_MXT_PLATFORM_MTK)
#else
	disable_irq_wake(irq);
#endif
}

static inline void board_disable_irq(const struct mxt_platform_data *pdata, unsigned int irq)
{
#if defined(CONFIG_MXT_PLATFORM_MTK)
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#else
	disable_irq(irq);
#endif
}

static inline void board_free_irq(const struct mxt_platform_data *pdata, unsigned int irq, void *dev_id)
{	
#if defined(CONFIG_MXT_PLATFORM_MTK)
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#else
	free_irq(irq,dev_id);
#endif
}

#endif /* __LINUX_ATMEL_MXT_TS_PLATFORM_H */
