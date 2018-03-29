/*
* Copyright (C) 2015 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef ICM20645_H
#define ICM20645_H

#include <linux/ioctl.h>

#define ICM20645_ACCESS_BY_GSE_I2C

#ifdef ICM20645_ACCESS_BY_GSE_I2C
    #define ICM20645_I2C_SLAVE_ADDR		(0xD2)   /* mtk i2c not allow to probe two same address */
#else
    #define ICM20645_I2C_SLAVE_ADDR		0xD0
#endif
#define REG_USER_CTRL           0x03

#define REG_BANK_SEL                    0x7F
#define BANK_SEL_0                      0x00
#define BANK_SEL_1                      0x10
#define BANK_SEL_2                      0x20
#define BANK_SEL_3                      0x30

#define ICM20645_REG_LP_CONFIG	        0x05
#define ICM20645_REG_PWR_CTL	        0x06
#define ICM20645_REG_POWER_CTL2         0x07

#define BIT_PWR_GYRO_STBY               0x07
#define BIT_GYRO_LP_EN                  0x10
#define BIT_LP_EN                       0x20
#define BIT_CLK_PLL                     0x01

/* ICM20645 Register Map  (Please refer to ICM20645 Specifications) */

#define ICM20645_REG_DEVID              0x00

#define ICM20645_REG_SAMRT_DIV          0x00
#define ICM20645_REG_CFG                0x01
/*set external sync, full-scale range and sample rate, low pass filter bandwidth*/
#define SHIFT_GYRO_FS_SEL                  1
/*full-scale range and sample rate*/
#define ICM20645_GYRO_CFG2              0x02

#define GYRO_AVGCFG_1X					   0
#define GYRO_AVGCFG_2X					   1
#define GYRO_AVGCFG_4X					   2
#define GYRO_AVGCFG_8X					   3
#define GYRO_AVGCFG_16X					   4
#define GYRO_AVGCFG_32X					   5
#define GYRO_AVGCFG_64X					   6
#define GYRO_AVGCFG_128X				   7

#define ICM20645_REG_GYRO_XH            0x33



#define GYRO_FS_SEL			         (ICM20645_FS_2000<<SHIFT_GYRO_FS_SEL)


#define ICM20645_FS_2000			 0x03

/* give more accurancy gyroscope rawdata, 2000 lsb is 32768 / 2000 = 16.4,
 * we magnify 10 times 16.4 and 131, then follows 164 and 1310
 */
#define ICM20645_FS_2000_LSB		   164
#define ICM20645_FS_MAX_LSB			  1310

#define GYRO_DLPFCFG	(7<<3)
#define GYRO_FCHOICE	0x01


#define ICM20645_SLEEP               0x40
/* FIFO */
#define REG_FIFO_EN             0x66
#define BIT_SLV_0_FIFO_EN               1
#define BIT_FIFO_EN                     0x40

#define REG_FIFO_EN_2           0x67
#define BIT_PRS_FIFO_EN                 0x20
#define BIT_ACCEL_FIFO_EN               0x10
#define BITS_GYRO_FIFO_EN               0x0E

#define REG_FIFO_RST            0x68

#define REG_FIFO_SIZE_0         0x6E
#define BIT_ACCEL_FIFO_SIZE_128         0x00
#define BIT_ACCEL_FIFO_SIZE_256         0x04
#define BIT_ACCEL_FIFO_SIZE_512         0x08
#define BIT_ACCEL_FIFO_SIZE_1024        0x0C
#define BIT_GYRO_FIFO_SIZE_128          0x00
#define BIT_GYRO_FIFO_SIZE_256          0x01
#define BIT_GYRO_FIFO_SIZE_512          0x02
#define BIT_GYRO_FIFO_SIZE_1024         0x03
#define BIT_FIFO_SIZE_1024              0x01
#define BIT_FIFO_SIZE_512               0x00
#define BIT_FIFO_3_SIZE_256             0x40
#define BIT_FIFO_3_SIZE_64              0x00

#define REG_FIFO_COUNT_H        0x70
#define REG_FIFO_COUNT_L        0x70
#define REG_FIFO_R_W            0x72

#define REG_FIFO_CFG            0x76
#define BIT_MULTI_FIFO_CFG              0x01
#define BIT_SINGLE_FIFO_CFG             0x00
#define BIT_GYRO_FIFO_NUM               (0 << 2)
#define BIT_ACCEL_FIFO_NUM              (1 << 2)
#define BIT_PRS_FIFO_NUM                2
#define BIT_EXT_FIFO_NUM                3
#define MAX_5_BIT_VALUE          0x1F


#define ICM20645_SUCCESS             0
#define ICM20645_ERR_I2C             -1
#define ICM20645_ERR_STATUS          -3
#define ICM20645_ERR_SETUP_FAILURE   -4
#define ICM20645_ERR_GETGSENSORDATA  -5
#define ICM20645_ERR_IDENTIFICATION  -6


#define ICM20645_BUFSIZE 60

/* 1 rad = 180/PI degree, MAX_LSB = 131, */
/* 180*131/PI = 7506 */
#define DEGREE_TO_RAD	7506

extern int ICM20645_gse_power(void);
extern int ICM20645_gse_mode(void);

#ifdef ICM20645_ACCESS_BY_GSE_I2C
extern int ICM20645_hwmsen_read_block(u8 addr, u8 *buf, u8 len);
extern int ICM20645_hwmsen_write_block(u8 addr, u8 *buf, u8 len);
#endif

#endif

