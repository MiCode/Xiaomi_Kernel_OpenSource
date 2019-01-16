/* mpu6050c.h
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * mpu300 head file for MT65xx
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef MPU60X0_H
#define MPU60X0_H
	 
#include <linux/ioctl.h>
	 
#define MPU6050C_ACC_I2C_SLAVE_ADDR		0xD0
#define MPU6050C_ACC_FIXED_DEVID		0xD0


/* MPU6050C_ACC Register Map  (Please refer to MPU6050C_ACC Specifications) */

#define  MPU6050C_ACC_REG_SELFTEST_X			0x0D
#define  MPU6050C_ACC_REG_SELFTEST_Y			0x0E
#define  MPU6050C_ACC_REG_SELFTEST_Z			0x0F
#define  MPU6050C_ACC_REG_SELFTEST_XYZ		0x10

#define  MPU6050C_ACC_REG_DATA_SMPRT_DIV	0x19
#define  MPU6050C_ACC_REG_DATA_CONFIG	0x1A
#define  MPU6050C_ACC_REG_GYRO_CONFIG	0x1B
#define	 MPU6050C_ACC_REG_ACC_CONFIG	0x1C

#define	 MPU6050C_ACC_REG_INT_CONFIG	0x37
#define	 MPU6050C_ACC_REG_INT_ENABLE	0x38
#define	 MPU6050C_ACC_REG_INT_STATUS	0x3A


#define  MPU6050C_ACC_REG_ACCEL_XOUT_H 		 0x3b
#define  MPU6050C_ACC_REG_ACCEL_XOUT_L 		 0x3c
#define  MPU6050C_ACC_REG_ACCEL_YOUT_H 		 0x3d
#define  MPU6050C_ACC_REG_ACCEL_YOUT_L 		 0x3e
#define  MPU6050C_ACC_REG_ACCEL_ZOUT_H 		 0x3f
#define  MPU6050C_ACC_REG_ACCEL_ZOUT_L 		 0x40

#define  MPU6050C_ACC_REG_TEMP_OUT_H 		 0x41
#define  MPU6050C_ACC_REG_TEMP_OUT_L 		 0x42

#define  MPU6050C_ACC_REG_GYRO_XOUT_H 		 0x43
#define  MPU6050C_ACC_REG_GYRO_XOUT_L 		 0x44
#define  MPU6050C_ACC_REG_GYRO_YOUT_H 		 0x45
#define  MPU6050C_ACC_REG_GYRO_YOUT_L 		 0x46
#define  MPU6050C_ACC_REG_GYRO_ZOUT_H 		 0x47
#define  MPU6050C_ACC_REG_GYRO_ZOUT_L 		 0x48

#define  MPU6050C_ACC_REG_PWR_MGMT_1	0x6B
#define  MPU6050C_ACC_REG_PWR_MGMT_2	0x6C

#define  MPU6050C_ACC_REG_DEVICE_ID	0x75	

 

/*MPU6050C_ACC Register Bit definitions*/ 

#define MPU6050C_FS_RANGE			0x03	//set the full-scale range of the gyro sensors
#define MPU6050C_FS_250 				0x00
#define MPU6050C_FS_500				0x01
#define MPU6050C_FS_1000				0x02
#define MPU6050C_FS_2000				0x03
#define MPU6050C_FS_MAX				0x03



#define MPU6050C_FS_250_LSB			131		// LSB/(o/s)
#define MPU6050C_FS_500_LSB			66
#define MPU6050C_FS_1000_LSB			33
#define MPU6050C_FS_2000_LSB			16
#define MPU6050C_FS_MAX_LSB			131


#define MPU6050C_ACC_SAM_RATE_MASK		0x07	//set sample rate and low padd filter configuration
#define MPU6050C_ACC_RATE_8K_LPFB_256HZ 	0x00
#define MPU6050C_ACC_RATE_1K_LPFB_188HZ	0x01
#define MPU6050C_ACC_RATE_1K_LPFB_98HZ 	0x02
#define MPU6050C_ACC_RATE_1K_LPFB_42HZ 	0x03
#define MPU6050C_ACC_RATE_1K_LPFB_20HZ 	0x04
#define MPU6050C_ACC_RATE_1K_LPFB_10HZ 	0x05
#define MPU6050C_ACC_RATE_1K_LPFB_5HZ 	0x06

#define MPU6050C_ACC_GYRO_RNG_250		0x0
#define MPU6050C_ACC_GYRO_RNG_500		0x1
#define MPU6050C_ACC_GYRO_RNG_1000		0x2
#define MPU6050C_ACC_GYRO_RNG_2000		0x3


#define MPU6050C_ACC_ACC_RNG_2G			0x0
#define MPU6050C_ACC_ACC_RNG_4G			0x1
#define MPU6050C_ACC_ACC_RNG_8G			0x2
#define MPU6050C_ACC_ACC_RNG_16G		0x3


#define MPU6050C_ACC_CLKSEL_8M		0x0
#define MPU6050C_ACC_CLKSEL_PLL_X		0x1
#define MPU6050C_ACC_CLKSEL_PLL_Y		0x2
#define MPU6050C_ACC_CLKSEL_PLL_Z		0x3
#define MPU6050C_ACC_CLKSEL_STOP		0x7

#define	MPU6050C_ACC_SLEEP	0x40
#define BIT_HW_RESET	0x80

	 
#define MPU6050C_ACC_SUCCESS				    0
#define MPU6050C_ACC_ERR_I2C				  -1
#define MPU6050C_ACC_ERR_STATUS			  -3
#define MPU6050C_ACC_ERR_SETUP_FAILURE	  -4
#define MPU6050C_ACC_ERR_GETGSENSORDATA  -5
#define MPU6050C_ACC_ERR_IDENTIFICATION	  -6
	 

/*
typedef enum  {
    MPU6050C_ACC_SYNC_NONE = 0x0,
    MPU6050C_ACC_SYNC_TEMP,
    MPU6050C_ACC_SYNC_GYROX,
    MPU6050C_ACC_SYNC_GYROY,
    MPU6050C_ACC_SYNC_GYROZ,
    MPU6050C_ACC_SYNC_AUXX,
    MPU6050C_ACC_SYNC_AUXY,
    MPU6050C_ACC_SYNC_AUXZ,
} MPU6050C_ACC_EXT_SYNC_SEL;

typedef enum  {
    MPU6050C_ACC_FS_250 = 0x0,
    MPU6050C_ACC_FS_500,
    MPU6050C_ACC_FS_1000,
    MPU6050C_ACC_FS_2000,
    MPU6050C_ACC_FS_MAX = 0x3,
} MPU6050C_ACC_FS_SEL;

typedef enum  {
    MPU6050C_ACC_RATE_8K_LPFB_256HZ = 0x0,
    MPU6050C_ACC_RATE_1K_LPFB_188HZ,
    MPU6050C_ACC_RATE_1K_LPFB_98HZ,
    MPU6050C_ACC_RATE_1K_LPFB_42HZ,
    MPU6050C_ACC_RATE_1K_LPFB_20HZ,
    MPU6050C_ACC_RATE_1K_LPFB_10HZ,
    MPU6050C_ACC_RATE_1K_LPFB_5HZ,
} MPU6050C_ACC_SAMRATE_SEL;
*/

#define MPU6050C_ACC_BUFSIZE 60

// 1 rad = 180/PI degree, MAX_LSB = 131,
// 180*131/PI = 7506
#define DEGREE_TO_RAD	7506

//extern struct mpu6050c_acc_i2c_data* MPU6050C_Acc_GetI2CData();

#endif //MPU60X0_H

