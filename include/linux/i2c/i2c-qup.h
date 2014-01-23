/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __I2C_QUP_H__
#define __I2C_QUP_H__

#ifdef CONFIG_I2C_QUP
int __init qup_i2c_init_driver(void);
#else
static inline int __init qup_i2c_init_driver(void) { return 0; }
#endif

#endif /* __I2C_QUP_H__ */
