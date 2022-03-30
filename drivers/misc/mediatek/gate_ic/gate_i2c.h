/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _GATE_I2C_DRV_H_
#define _GATE_I2C_DRV_H_

//extern int rt4831_read_byte(unsigned char cmd, unsigned char *returnData);
extern int _gate_ic_i2c_write_bytes(unsigned char cmd, unsigned char writeData);
extern int _gate_ic_i2c_read_bytes(unsigned char cmd, unsigned char *returnData);
extern void _gate_ic_i2c_panel_bias_enable(unsigned int power_status);
extern void _gate_ic_Power_on(void);
extern void _gate_ic_Power_off(void);
extern int _gate_ic_backlight_set(unsigned int level);


#endif

