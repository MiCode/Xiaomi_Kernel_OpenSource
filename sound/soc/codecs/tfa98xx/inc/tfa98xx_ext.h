/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __TFA98XX_EXT_INC__
#define __TFA98XX_EXT_INC__
#include <linux/i2c.h>

extern int tfa98xx_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id);
extern int tfa98xx_i2c_remove(struct i2c_client *i2c);

#endif /* __TFA98XX_EXT_INC__ */

