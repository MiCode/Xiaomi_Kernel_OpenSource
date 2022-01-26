/*
 * Filename: lcm_cust_common.c
 * date:20201209
 * Description: cust common source file
 * Author:samir.liu
 */

#ifndef __LCM_CUST_COMMON_H__
#define __LCM_CUST_COMMON_H__

#define LP36273_DISP_REV		0x01
#define LP36273_DISP_BC1		0x02
#define LP36273_DISP_BC2		0x03
#define LP36273_DISP_BB_LSB		0x04
#define LP36273_DISP_BB_MSB		0x05
#define LP36273_DISP_BAFLT		0x06
#define LP36273_DISP_BAFHT		0x07
#define LP36273_DISP_BL_ENABLE	0x08
#define LP36273_DISP_BIAS_CONF1	0x09
#define LP36273_DISP_BIAS_CONF2	0x0a
#define LP36273_DISP_BIAS_CONF3	0x0b
#define LP36273_DISP_BIAS_BOOST	0x0c
#define LP36273_DISP_BIAS_VPOS	0x0d
#define LP36273_DISP_BIAS_VNEG	0x0e
#define LP36273_DISP_FLAGS		0x0f
#define LP36273_DISP_OPTION1	0x10
#define LP36273_DISP_OPTION2	0x11
#define LP36273_DISP_PTD_LSB	0x12
#define LP36273_DISP_PTD_MSB	0x13
#define LP36273_DISP_FULL_CURRENT	0x15

#define DISPPARAM_LCD_HBM_L1_ON		1
#define DISPPARAM_LCD_HBM_L2_ON		2
#define DISPPARAM_LCD_HBM_L3_ON		3
#define DISPPARAM_LCD_HBM_OFF		0
#define BL_LEVEL_MAX 2047

#define LM36273_ADDR 			0x11
#define LM36273_I2C_ID 			0x06

#define BL_HBM_L1 1533/* 22.1mA for exponential mapping*/
#define BL_HBM_L2 1622/* 23.6mA for exponential mapping*/
#define BL_HBM_L3 1726/* 25.0mA for exponential mapping*/
#define BL_HBM_L3_2 1750

int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value);
int lm36273_bl_bias_conf(void);
int lm36273_bias_enable(int enable, int delayMs);
int lm36273_brightness_set(int level);
int hbm_brightness_set(int level);
int hbm_brightness_get(void);


#endif
