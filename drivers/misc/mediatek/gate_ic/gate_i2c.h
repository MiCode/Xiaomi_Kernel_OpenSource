/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _GATE_I2C_DRV_H_
#define _GATE_I2C_DRV_H_

//extern int rt4831_read_byte(unsigned char cmd, unsigned char *returnData);
extern int _gate_ic_i2c_write_bytes(unsigned char cmd, unsigned char writeData);
extern int _gate_ic_i2c_read_bytes(unsigned char cmd, unsigned char *returnData);
extern void _gate_ic_i2c_panel_bias_enable(unsigned int power_status);
extern void _gate_ic_Power_on(void);
extern void _gate_ic_Power_off(void);
extern void _gate_ic_backlight_set(unsigned int level);


#endif

