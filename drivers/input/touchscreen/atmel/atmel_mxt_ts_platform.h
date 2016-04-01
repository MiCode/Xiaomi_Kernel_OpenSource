/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Copyright (C) 2016 XiaoMi, Inc.
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
#define CONFIG_MXT_VENDOR_ID_BY_T19

#	define CONFIG_FB_PM
#	define CONFIG_MXT_IRQ_NESTED

#define CONFIG_DUMMY_PARSE_DTS

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

static inline int board_gpio_init(const struct mxt_platform_data *pdata)
{
	int err = 0;
	static int once_time;

	/* set irq pin input/pull high */
	/*	 power off vdd/avdd   */
	/*		msleep(100);		*/
	/* 	  set reset output 0 	*/
	/*	 power up vdd/avdd	*/
	/*		msleep(50);			*/
	/*	   set reset output 1 	*/
	/*		msleep(200);		*/
	if (!once_time) {
		if (gpio_is_valid(pdata->irq_gpio)) {
			err = gpio_request(pdata->irq_gpio, "atmel_irq_gpio");
			if (err) {
				printk("board_gpio_init  irq gpio request failed\n");
			goto free_irq_gpio;
			}
		}

		if (gpio_is_valid(pdata->gpio_reset)) {
			err = gpio_request(pdata->gpio_reset, "atmel_gpio_reset");
			if (err) {
				printk("board_gpio_init  reset gpio request failed\n");
				goto free_gpio_reset;
			}
		}
		once_time = 1;
	}
	err = gpio_direction_input(pdata->irq_gpio);
		if (err) {
			printk("set_direction for irq gpio failed\n");
			goto free_irq_gpio;
		}
	err = gpio_direction_output(pdata->gpio_reset, 0);
		if (err) {
			printk("set_direction for reset gpio failed\n");
			goto free_gpio_reset;
		}
		return err;

	free_gpio_reset:
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);
	free_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio)) {
		printk("%s, free_irq_gpio, irq_gpio=%ld\n", __func__, pdata->irq_gpio);
		gpio_free(pdata->irq_gpio);
	}
	return err;

}
static inline void board_gpio_deinit(const struct mxt_platform_data *pdata)
{
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);
	if (gpio_is_valid(pdata->irq_gpio))
		printk("%s, free_irq_gpio, irq_gpio=%ld\n", __func__, pdata->irq_gpio);
		gpio_free(pdata->irq_gpio);


}
static void board_init_irq(const struct mxt_platform_data *pdata)
{

}

static inline void board_enable_irq(const struct mxt_platform_data *pdata, unsigned int irq)
{
	enable_irq(irq);
}

static inline void board_enable_irq_wake(const struct mxt_platform_data *pdata, unsigned int irq)
{
	enable_irq_wake(irq);
}

static inline void board_disable_irq_wake(const struct mxt_platform_data *pdata, unsigned int irq)
{
	disable_irq_wake(irq);
}

static inline void board_disable_irq(const struct mxt_platform_data *pdata, unsigned int irq)
{
	disable_irq(irq);
}

static inline void board_free_irq(const struct mxt_platform_data *pdata, unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

#endif /* __LINUX_ATMEL_MXT_TS_PLATFORM_H */
