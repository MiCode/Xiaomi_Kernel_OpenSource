/* drivers/input/misc/ots_pat9125/pixart_platform.h
 *
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __PIXART_PLATFORM_H_
#define __PIXART_PLATFORM_H_

#include <linux/i2c.h>
#include <linux/delay.h>

/* extern functions */
extern unsigned char read_data(struct i2c_client *, u8 addr);
extern void write_data(struct i2c_client *, u8 addr, u8 data);

#endif
