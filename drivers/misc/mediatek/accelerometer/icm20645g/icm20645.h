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

#define ICM20645_I2C_SLAVE_ADDR		    0xD0


#define REG_BANK_SEL                    0x7F
#define BANK_SEL_0                      0x00
#define BANK_SEL_1                      0x10
#define BANK_SEL_2                      0x20
#define BANK_SEL_3                      0x30


/* ICM20645 Register Map  (Please refer to ICM20645 Specifications) */
#define ICM20645_REG_DEVID			    0x00
#define ICM20645_REG_LP_CONFIG	        0x05
#define ICM20645_REG_POWER_CTL          0x06
#define ICM20645_REG_POWER_CTL2         0x07


#define ICM20645_REG_SAMRT_DIV1         0x10
#define	ICM20645_REG_SAMRT_DIV2			0x11

#define	BIT_ACC_LP_EN					0x20
#define BIT_ACC_I2C_MST                 0x40

#define BIT_LP_EN                       0x20
#define BIT_CLK_PLL                     0x01
#define BIT_TEMP_DIS                    (1<<3)

#define BIT_PWR_ACCEL_STBY              0x38
#define BIT_PWR_GYRO_STBY               0x07

#define	ICM20645_REG_INT_ENABLE			0x11
#define	ICM20645_ACC_CONFIG				0x14
#define ICM20645_ACC_CONFIG_2		    0x15
#define ACCEL_FCHOICE                      1
#define ACCEL_DLPFCFG                   (7<<3)

#define ACCEL_AVGCFG_1_4X				   0
#define ACCEL_AVGCFG_8X					   1
#define ACCEL_AVGCFG_16X				   2
#define ACCEL_AVGCFG_32X				   3

#define	ICM20645_REG_DATAX0				0x2d
#define	ICM20645_REG_DATAY0				0x2f
#define	ICM20645_REG_DATAZ0				0x31


#define ICM20645_RANGE_2G			(0x00 << 1)
#define ICM20645_RANGE_4G			(0x01 << 1)
#define ICM20645_RANGE_8G			(0x02 << 1)
#define ICM20645_RANGE_16G			(0x03 << 1)



#define ICM20645_SLEEP				0x40
#define ICM20645_DEV_RESET           0x80



#define ICM20645_SUCCESS                     0
#define ICM20645_ERR_I2C                     -1
#define ICM20645_ERR_STATUS                  -3
#define ICM20645_ERR_SETUP_FAILURE           -4
#define ICM20645_ERR_GETGSENSORDATA          -5
#define ICM20645_ERR_IDENTIFICATION          -6



#define ICM20645_BUFSIZE				256

extern int ICM20645_gyro_power(void);
extern int ICM20645_gyro_mode(void);

#endif

