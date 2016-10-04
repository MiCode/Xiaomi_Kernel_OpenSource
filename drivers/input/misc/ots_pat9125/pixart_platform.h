/* drivers/input/misc/ots_pat9125/pixart_platform.h
 *
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 */

#ifndef _PIXART_PLATFORM_
#define _PIXART_PLATFORM_

#include <linux/input.h>
#include <linux/pm.h>
#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/types.h>

/* extern functions */
extern unsigned char ReadData(unsigned char addr);
extern void WriteData(unsigned char addr, unsigned char data);

#endif
