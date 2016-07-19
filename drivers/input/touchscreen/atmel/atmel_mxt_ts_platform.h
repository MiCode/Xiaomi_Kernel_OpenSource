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


#define CONFIG_MXT_IRQ_WORKQUEUE
#define CONFIG_MXT_PLUGIN_SUPPORT
#define CONFIG_MXT_PROBE_ALTERNATIVE_CHIP 8


#define CONFIG_MXT_VENDOR_ID_BY_T19




#	define CONFIG_FB_PM
#	define CONFIG_MXT_IRQ_NESTED


#include <linux/io.h>
#include <mach/gpio.h>

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

#if defined(CONFIG_MXT_EXTERNAL_TRIGGER_IRQ_WORKQUEUE)
static struct mxt_data *mxt_g_data;
#endif


static inline void board_gpio_init(const struct mxt_platform_data *pdata)
{
	int  err = 0;


	/* set irq pin input/pull high */
	/*	 power off vdd/avdd   */
	/*		msleep(100);		*/
	/* 	  set reset output 0 	*/
	/*	 power up vdd/avdd	*/
	/*		msleep(50);			*/
	/*	   set reset output 1 	*/
	/*		msleep(200);		*/



	if (gpio_is_valid(pdata->irq_gpio)) {
		err = gpio_request(pdata->irq_gpio, "atmel_irq_gpio");
		if (err) {
			printk("irq gpio request failed\n");
		}
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		err = gpio_request(pdata->reset_gpio, "atmel_reset_gpio");
		if (err) {
			printk("reset gpio request failed\n");
		}
	}
	err = gpio_direction_input(pdata->irq_gpio);
		if (err) {
			printk("set_direction for irq gpio failed\n");
			goto free_irq_gpio;
		}
	err = gpio_direction_output(pdata->reset_gpio, 1);
		if (err) {
			printk("set_direction for reset gpio failed\n");
			goto free_reset_gpio;
		}
		gpio_set_value(pdata->reset_gpio, 0);
		msleep(10);
		gpio_set_value(pdata->reset_gpio, 1);
		msleep(200);

free_reset_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
free_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio)) {
		printk("%s, free_irq_gpio, irq_gpio=%ld\n", __func__, pdata->irq_gpio);
		gpio_free(pdata->irq_gpio);
	}
}
static inline void board_gpio_deinit(const struct mxt_platform_data *pdata)
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
