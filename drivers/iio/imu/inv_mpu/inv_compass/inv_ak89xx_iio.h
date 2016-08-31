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
*/
#ifndef _INV_AK89XX_IIO_H_
#define _INV_AK89XX_IIO_H_

#include <linux/i2c.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/spinlock.h>
#include <linux/mpu.h>

#include "iio.h"
#include "buffer.h"
#include "trigger.h"

/**
 *  struct inv_ak89xx_state_s - Driver state variables.
 *  @plat_data:     board file platform data.
 *  @i2c:           i2c client handle.
 *  @trig:          not used. for compatibility.
 *  @work:          work data structure.
 *  @delay:         delay between each scheduled work.
 *  @dev:           Represents read-only node for accessing buffered data.
 *  @inv_dev:       Handle to input device.
 *  @sl_handle:		Handle to I2C port.
 */
struct inv_ak89xx_state_s {
	struct mpu_platform_data plat_data;
	struct i2c_client *i2c;
	struct iio_trigger  *trig;
	struct delayed_work work;
	int delay;                 /* msec */
	unsigned char compass_id;
	int compass_scale;         /* for ak8963, 1:16-bit, 0:14-bit */
	short compass_data[3];
	u8 asa[3];	           /* axis sensitivity adjustment */
	s64 timestamp;
	short i2c_addr;
	void *sl_handle;
	struct device *inv_dev;
	struct input_dev *idev;
};

/* scan element definition */
enum inv_mpu_scan {
	INV_AK89XX_SCAN_MAGN_X,
	INV_AK89XX_SCAN_MAGN_Y,
	INV_AK89XX_SCAN_MAGN_Z,
	INV_AK89XX_SCAN_TIMESTAMP,
};

#define AK89XX_I2C_NAME "ak89xx"

#define SENSOR_DATA_SIZE    8
#define YPR_DATA_SIZE       12
#define RWBUF_SIZE          16

#define ACC_DATA_FLAG       0
#define MAG_DATA_FLAG       1
#define ORI_DATA_FLAG       2
#define AKM_NUM_SENSORS     3

#define ACC_DATA_READY		(1 << (ACC_DATA_FLAG))
#define MAG_DATA_READY		(1 << (MAG_DATA_FLAG))
#define ORI_DATA_READY		(1 << (ORI_DATA_FLAG))

#define AKM_MINOR_NUMBER	254

/*! \name AK89XX constant definition
 \anchor AK89XX_Def
 Constant definitions of the AK89XX.*/
#define AK89XX_MEASUREMENT_TIME_US	10000

/*! \name AK89XX operation mode
 \anchor AK89XX_Mode
 Defines an operation mode of the AK89XX.*/
/*! @{*/
#define AK89XX_CNTL_MODE_SNG_MEASURE    0x01
#define	AK89XX_CNTL_MODE_SELF_TEST      0x08
#define	AK89XX_CNTL_MODE_FUSE_ACCESS    0x0F
#define	AK89XX_CNTL_MODE_POWER_DOWN     0x00
/*! @}*/

/*! \name AK89XX register address
\anchor AK89XX_REG
Defines a register address of the AK89XX.*/
/*! @{*/
#define AK89XX_REG_WIA		0x00
#define AK89XX_REG_INFO		0x01
#define AK89XX_REG_ST1		0x02
#define AK89XX_REG_HXL		0x03
#define AK89XX_REG_HXH		0x04
#define AK89XX_REG_HYL		0x05
#define AK89XX_REG_HYH		0x06
#define AK89XX_REG_HZL		0x07
#define AK89XX_REG_HZH		0x08
#define AK89XX_REG_ST2		0x09
#define AK89XX_REG_CNTL		0x0A
#define AK89XX_REG_RSV		0x0B
#define AK89XX_REG_ASTC		0x0C
#define AK89XX_REG_TS1		0x0D
#define AK89XX_REG_TS2		0x0E
#define AK89XX_REG_I2CDIS	0x0F
/*! @}*/

/*! \name AK89XX fuse-rom address
\anchor AK89XX_FUSE
Defines a read-only address of the fuse ROM of the AK89XX.*/
/*! @{*/
#define AK89XX_FUSE_ASAX	0x10
#define AK89XX_FUSE_ASAY	0x11
#define AK89XX_FUSE_ASAZ	0x12
/*! @}*/

#define AK89XX_MAX_DELAY        (200)
#define AK89XX_MIN_DELAY        (10)
#define AK89XX_DEFAULT_DELAY    (100)

#define INV_ERROR_COMPASS_DATA_OVERFLOW  (-1)
#define INV_ERROR_COMPASS_DATA_NOT_READY (-2)

int inv_ak89xx_configure_ring(struct iio_dev *indio_dev);
void inv_ak89xx_unconfigure_ring(struct iio_dev *indio_dev);
int inv_ak89xx_probe_trigger(struct iio_dev *indio_dev);
void inv_ak89xx_remove_trigger(struct iio_dev *indio_dev);
void set_ak89xx_enable(struct iio_dev *indio_dev, bool enable);
int ak89xx_read_raw_data(struct inv_ak89xx_state_s *st,
				short dat[3]);
void inv_read_ak89xx_fifo(struct iio_dev *indio_dev);
int ak89xx_read(struct inv_ak89xx_state_s *st, short rawfixed[3]);

#endif

