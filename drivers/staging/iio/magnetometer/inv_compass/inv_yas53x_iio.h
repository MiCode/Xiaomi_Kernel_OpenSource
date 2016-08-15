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
 *      @file  inv_yas53x_iio.h
 *      @brief Struct definitions for the Invensense implementation
 *              of yas53x driver.
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

#define YAS_MAG_MAX_FILTER_LEN			30
struct yas_adaptive_filter {
	int num;
	int index;
	int filter_len;
	int filter_noise;
	int sequence[YAS_MAG_MAX_FILTER_LEN];
};

struct yas_thresh_filter {
	int threshold;
	int last;
};

struct yas_filter {
	int filter_len;
	int filter_thresh;
	int filter_noise[3];
	struct yas_adaptive_filter adap_filter[3];
	struct yas_thresh_filter thresh_filter[3];
};

/**
 *  struct inv_compass_state - Driver state variables.
 *  @plat_data:         mpu platform data from board file.
 *  @client:		i2c client handle.
 *  @chan_info:         channel information.
 *  @trig:              IIO trigger.
 *  @work:              work structure.
 *  @delay:             delay to schedule the next work.
 *  @overflow_bound:    bound to determine overflow.
 *  @center:            center of the measurement.
 *  @compass_data[3]:   compass data store.
 *  @offset[3]:         yas530 specific data.
 *  @base_compass_data[3]: first measure data after reset.
 *  @first_measure_after_reset:1: flag for first measurement after reset.
 *  @first_read_after_reset:1: flag for first read after reset.
 *  @reset_timer: timer to accumulate overflow conditions.
 *  @overunderflow:1:     overflow and underflow flag.
 *  @filter: filter data structure.
 *  @read_data:         function pointer of reading data from device.
 *  @get_cal_data: function pointer of reading cal data.
 */
struct inv_compass_state {
	struct mpu_platform_data plat_data;
	struct i2c_client *client;
	struct iio_trigger  *trig;
	struct delayed_work work;
	s16 delay;
	s16 overflow_bound;
	s16 upper_bound;
	s16 lower_bound;
	s16 center;
	s16 compass_data[3];
	s8 offset[3];
	s16 base_compass_data[3];
	u8 first_measure_after_reset:1;
	u8 first_read_after_reset:1;
	u8 overunderflow:1;
	s32 reset_timer;
	struct yas_filter filter;
	int (*read_data)(struct inv_compass_state *st,
			  int *, u16 *, u16 *, u16 *, u16 *);
	int (*get_cal_data)(struct inv_compass_state *);
};

/* scan element definition */
enum inv_mpu_scan {
	INV_YAS53X_SCAN_MAGN_X,
	INV_YAS53X_SCAN_MAGN_Y,
	INV_YAS53X_SCAN_MAGN_Z,
	INV_YAS53X_SCAN_TIMESTAMP,
};

#define YAS530_REGADDR_DEVICE_ID          0x80
#define YAS530_REGADDR_ACTUATE_INIT_COIL  0x81
#define YAS530_REGADDR_MEASURE_COMMAND    0x82
#define YAS530_REGADDR_CONFIG             0x83
#define YAS530_REGADDR_MEASURE_INTERVAL   0x84
#define YAS530_REGADDR_OFFSET_X           0x85
#define YAS530_REGADDR_OFFSET_Y1          0x86
#define YAS530_REGADDR_OFFSET_Y2          0x87
#define YAS530_REGADDR_TEST1              0x88
#define YAS530_REGADDR_TEST2              0x89
#define YAS530_REGADDR_CAL                0x90
#define YAS530_REGADDR_MEASURE_DATA       0xb0

#define YAS530_MAX_DELAY                  200
#define YAS530_MIN_DELAY                  5
#define YAS530_SCALE                      107374182L

#define YAS_YAS530_VERSION_A		0	/* YAS530  (MS-3E Aver) */
#define YAS_YAS530_VERSION_B		1	/* YAS530B (MS-3E Bver) */
#define YAS_YAS530_VERSION_A_COEF	380
#define YAS_YAS530_VERSION_B_COEF	550
#define YAS_YAS530_DATA_CENTER		2048
#define YAS_YAS530_DATA_OVERFLOW	4095
#define YAS_YAS530_CAL_DATA_SIZE        16

/*filter related defines */
#define YAS_MAG_DEFAULT_FILTER_NOISE_X          144 /* sd: 1200 nT */
#define YAS_MAG_DEFAULT_FILTER_NOISE_Y          144 /* sd: 1200 nT */
#define YAS_MAG_DEFAULT_FILTER_NOISE_Z          144 /* sd: 1200 nT */
#define YAS_MAG_DEFAULT_FILTER_LEN              20

#define YAS530_MAG_DEFAULT_FILTER_THRESH        100
#define YAS532_MAG_DEFAULT_FILTER_THRESH        300

#define YAS_YAS532_533_VERSION_AB	0 /* YAS532_533AB (MS-3R/3F ABver) */
#define YAS_YAS532_533_VERSION_AC	1 /* YAS532_533AC (MS-3R/3F ACver) */
#define YAS_YAS532_533_VERSION_AB_COEF	1800
#define YAS_YAS532_533_VERSION_AC_COEF	900
#define YAS_YAS532_533_DATA_CENTER      4096
#define YAS_YAS532_533_DATA_OVERFLOW	8190
#define YAS_YAS532_533_CAL_DATA_SIZE    14

#define YAS_MAG_DISTURBURNCE_THRESHOLD  1600
#define YAS_RESET_COIL_TIME_THRESHOLD   3000

#define INV_ERROR_COMPASS_DATA_OVERFLOW  (-1)
#define INV_ERROR_COMPASS_DATA_NOT_READY (-2)

int inv_yas53x_configure_ring(struct iio_dev *indio_dev);
void inv_yas53x_unconfigure_ring(struct iio_dev *indio_dev);
int inv_yas53x_probe_trigger(struct iio_dev *indio_dev);
void inv_yas53x_remove_trigger(struct iio_dev *indio_dev);
void set_yas53x_enable(struct iio_dev *indio_dev, bool enable);
void inv_read_yas53x_fifo(struct iio_dev *indio_dev);
int yas53x_read(struct inv_compass_state *st, short rawfixed[3],
				s32 *overunderflow);
int yas53x_resume(struct inv_compass_state *st);

#endif  /* #ifndef _INV_GYRO_H_ */

