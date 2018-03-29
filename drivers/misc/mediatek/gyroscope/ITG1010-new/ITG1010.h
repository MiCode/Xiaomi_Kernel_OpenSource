/*
* Copyright(C)2014 MediaTek Inc.
* Modification based on code covered by the below mentioned copyright
* and/or permission notice(S).
*/

/* ITG1010 motion sensor driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef ITG1010_H
#define ITG1010_H

#include <linux/ioctl.h>

#define ITG1010_I2C_SLAVE_ADDR        0xD0


/* ITG1010 Register Map  (Please refer to ITG1010 Specifications) */

#define ITG1010_REG_DEVID           0x75
#define ITG1010_REG_FIFO_EN         0x23
#define ITG1010_REG_AUX_VDD         0x01

#define ITG1010_REG_SAMRT_DIV       0x19
/* set external sync, full-scale range and sample rate, low pass filter bandwidth */
#define ITG1010_REG_CFG             0x1A
#define ITG1010_REG_GYRO_CFG        0x1B      /* full-scale range and sample rate, */


#define ITG1010_REG_GYRO_XH         0x43

#define ITG1010_REG_TEMPH           0x41


#define ITG1010_REG_FIFO_CNTH       0x72
#define ITG1010_REG_FIFO_CNTL       0x73
#define ITG1010_REG_FIFO_DATA       0x74
#define ITG1010_REG_FIFO_CTL        0x6A
#define ITG1010_REG_PWR_CTL            0x6B
#define ITG1010_REG_PWR_CTL2        0x6C


/*ITG1010 Register Bit definitions*/

#define ITG1010_FIFO_GYROX_EN       0x40    /* insert the X Gyro data into FIFO */
#define ITG1010_FIFO_GYROY_EN       0x20    /* insert the Y Gyro data into FIFO */
#define ITG1010_FIFO_GYROZ_EN       0x10    /* insert the Z Gyro data into FIFO */

#define ITG1010_AUX_VDDIO_DIS       0x00    /* disable VDD level for the secondary I2C bus clock and data lines */

/* for ITG1010_REG_CFG */
/* 0x05    //captue the state of external frame sync input pin to insert into LSB of registers */
#define ITG1010_EXT_SYNC            0x03
#define ITG1010_SYNC_GYROX            0x02

/* for ITG1010_REG_GYRO_CFG */
#define ITG1010_FS_RANGE            0x03    /* set the full-scale range of the gyro sensors */
#define ITG1010_FS_1000                0x02


#define ITG1010_FS_1000_LSB            33
#define ITG1010_FS_MAX_LSB            131

#define ITG1010_RATE_1K_LPFB_188HZ    0x01
#define ITG1010_RATE_1K_LPFB_256HZ    0x00

#define ITG1010_FIFO_EN                0x40    /* enable FIFO operation for sensor data */

#define ITG1010_FIFO_RST            0x40    /* reset FIFO function */

#define ITG1010_SLEEP               0x40    /* enable low power sleep mode */


#define ITG1010_SUCCESS             0
#define ITG1010_ERR_I2C             -1
#define ITG1010_ERR_STATUS          -3
#define ITG1010_ERR_SETUP_FAILURE   -4
#define ITG1010_ERR_GETGSENSORDATA  -5
#define ITG1010_ERR_IDENTIFICATION  -6


#define ITG1010_BUFSIZE 60

/* 1 rad = 180/PI degree, MAX_LSB = 131, */
/* 180*131/PI = 7506 */
#define DEGREE_TO_RAD    7506

#ifdef CONFIG_CUSTOM_KERNEL_GYROSCOPE_MODULE
extern bool gyro_success_Flag;
#endif

#endif /* ITG1010_H */
