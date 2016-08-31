/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (C) 2016 XiaoMi, Inc.
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

/**
 *  @addtogroup DRIVERS
 *  @brief      Hardware drivers.
 *
 *  @{
 *      @file  inv_ami306_iio.h
 *      @brief Struct definitions for the Invensense implementation
 *              of ami306 driver.
 */

#ifndef _INV_GYRO_H_
#define _INV_GYRO_H_

#include <linux/i2c.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/spinlock.h>
#include <linux/mpu.h>

#include "iio.h"
#include "buffer.h"
#include "trigger.h"

/** axis sensitivity(gain) calibration parameter information  */
struct ami_vector3d {
	signed short x;                 /**< X-axis  */
	signed short y;                 /**< Y-axis  */
	signed short z;                 /**< Z-axis  */
};

/** axis interference information  */
struct ami_interference {
	/**< Y-axis magnetic field for X-axis correction value  */
	signed short xy;
	/**< Z-axis magnetic field for X-axis correction value  */
	signed short xz;
	/**< X-axis magnetic field for Y-axis correction value  */
	signed short yx;
	/**< Z-axis magnetic field for Y-axis correction value  */
	signed short yz;
	/**< X-axis magnetic field for Z-axis correction value  */
	signed short zx;
	/**< Y-axis magnetic field for Z-axis correction value  */
	signed short zy;
};

/** sensor calibration Parameter information  */
struct ami_sensor_parametor {
	/**< geomagnetic field sensor gain  */
	struct ami_vector3d m_gain;
	/**< geomagnetic field sensor gain correction parameter  */
	struct ami_vector3d m_gain_cor;
	/**< geomagnetic field sensor offset  */
	struct ami_vector3d m_offset;
	/**< geomagnetic field sensor axis interference parameter */
	struct ami_interference m_interference;
};

/**
 *  struct inv_ami306_state_s - Driver state variables.
 *  @plat_data:         board file platform data.
 *  @i2c:		i2c client handle.
 *  @trig:              not used. for compatibility.
 *  @param:             ami specific sensor data.
 *  @work:              work data structure.
 *  @delay:             delay between each scheduled work.
 *  @fine:               fine tunign parameters.
 *  @compass_data:      compass data store.
 *  @timestamp:         time stamp.
 */
struct inv_ami306_state_s {
	struct mpu_platform_data plat_data;
	struct i2c_client *i2c;
	struct iio_trigger  *trig;
	struct ami_sensor_parametor param;
	struct delayed_work work;
	int delay;
	s8 fine[3];
	short compass_data[3];
	s64 timestamp;
};
/* scan element definition */
enum inv_mpu_scan {
	INV_AMI306_SCAN_MAGN_X,
	INV_AMI306_SCAN_MAGN_Y,
	INV_AMI306_SCAN_MAGN_Z,
	INV_AMI306_SCAN_TIMESTAMP,
};

#define REG_AMI_WIA                     0x0f
#define REG_AMI_DATAX                   0x10
#define REG_AMI_STA1                    0x18
#define REG_AMI_CTRL1                   0x1b
#define REG_AMI_CTRL2                   0x1c
#define REG_AMI_CTRL3                   0x1d
#define REG_AMI_B0X                     0x20
#define REG_AMI_B0Y                     0x22
#define REG_AMI_B0Z                     0x24
#define REG_AMI_CTRL5                   0x40
#define REG_AMI_CTRL4                   0x5c
#define REG_AMI_TEMP                    0x60
#define REG_AMI_SENX                    0x96
#define REG_AMI_OFFX                    0x6c
#define REG_AMI_OFFY                    0x72
#define REG_AMI_OFFZ                    0x78


#define DATA_WIA                        0x46
#define AMI_CTRL1_PC1                   0x80
#define AMI_CTRL1_FS1_FORCE             0x02
#define AMI_CTRL1_ODR1                  0x10
#define AMI_CTRL2_DREN                  0x08
#define AMI_CTRL2_DRP                   0x04
#define AMI_CTRL3_FORCE_BIT             0x40
#define AMI_CTRL3_B0_LO_BIT             0x10
#define AMI_CTRL3_SRST_BIT              0x80
#define AMI_CTRL4_HS                    0xa07e
#define AMI_CTRL4_AB                    0x0001
#define AMI_STA1_DRDY_BIT               0x40
#define AMI_STA1_DOR_BIT                0x20

#define AMI_PARAM_LEN                   12
#define AMI_STANDARD_OFFSET             0x800
#define AMI_GAIN_COR_DEFAULT            1000
#define AMI_FINE_MAX                    96
#define AMI_MAX_DELAY                   1000
#define AMI_MIN_DELAY                   10
#define AMI_SCALE                       (5461 * (1<<15))

#define INV_ERROR_COMPASS_DATA_OVERFLOW  (-1)
#define INV_ERROR_COMPASS_DATA_NOT_READY (-2)


int inv_ami306_configure_ring(struct iio_dev *indio_dev);
void inv_ami306_unconfigure_ring(struct iio_dev *indio_dev);
int inv_ami306_probe_trigger(struct iio_dev *indio_dev);
void inv_ami306_remove_trigger(struct iio_dev *indio_dev);
int set_ami306_enable(struct iio_dev *indio_dev, int state);
int ami306_read_raw_data(struct inv_ami306_state_s *st,
				short dat[3]);
int inv_read_ami306_fifo(struct iio_dev *indio_dev);

#endif  /* #ifndef _INV_GYRO_H_ */

