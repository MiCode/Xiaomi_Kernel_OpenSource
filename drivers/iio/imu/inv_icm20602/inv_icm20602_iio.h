/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _INV_ICM20602_IIO_H_
#define _INV_ICM20602_IIO_H_

#include <linux/i2c.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/platform_data/invensense_mpu6050.h>

/*
 * it uses sensor irq to trigger
 * if set INV20602_DEVICE_IRQ_TRIGGER as 1,
 * otherwise use SMD IRQ to trigger
 */
#define INV20602_DEVICE_IRQ_TRIGGER    0
extern int icm20602_debug_enable;

#if INV20602_DEVICE_IRQ_TRIGGER
#define INV20602_SMD_IRQ_TRIGGER    0
#else
#define INV20602_SMD_IRQ_TRIGGER    1
#endif


#define dev_dbginfo(fmt, ...) \
	do { \
		if (icm20602_debug_enable > 0) { \
			printk(fmt, ##__VA_ARGS__);	  \
		} \
	} while (0)

#define dev_dbgerr(fmt, ...) printk(fmt, ##__VA_ARGS__)
#define INV_ICM20602_TIME_STAMP_TOR           5
#define ICM20602_PACKAGE_SIZE 14

/* device enum */
enum inv_devices {
	INV_ICM20602,
	INV_NUM_PARTS
};

enum _mpu_err {
	MPU_SUCCESS = 0,
	MPU_FAIL = 1,
	MPU_READ_FAIL = 2,
	MPU_WRITE_FAIL = 3,
};

/* Gyro Full Scale Range Enum */
enum inv_icm20602_gyro_fsr_e {
	ICM20602_GYRO_FSR_250DPS = 0,
	ICM20602_GYRO_FSR_500DPS,
	ICM20602_GYRO_FSR_1000DPS,
	ICM20602_GYRO_FSR_2000DPS,
	ICM20602_GYRO_FSR_NUM
};

/* Accelerometor Full Scale Range Enum */
enum inv_icm20602_acc_fsr_e {
	ICM20602_ACC_FSR_2G = 0,
	ICM20602_ACC_FSR_4G,
	ICM20602_ACC_FSR_8G,
	ICM20602_ACC_FSR_16G,
	ICM20602_ACC_FSR_NUM
};

/* scan element definition */
enum inv_icm20602_scan {
	INV_ICM20602_SCAN_ACCL_X,
	INV_ICM20602_SCAN_ACCL_Y,
	INV_ICM20602_SCAN_ACCL_Z,
	INV_ICM20602_SCAN_GYRO_X,
	INV_ICM20602_SCAN_GYRO_Y,
	INV_ICM20602_SCAN_GYRO_Z,
	INV_ICM20602_SCAN_TEMP,
	INV_ICM20602_SCAN_TIMESTAMP,
};

/* this is for CONFIGURATION reg bit[2:0] DLFP_CFG */
enum inv_icm20602_gyro_temp_lpf_e {
	INV_ICM20602_GLFP_250HZ = 0,  /* 8KHz */
	INV_ICM20602_GYRO_LFP_176HZ,      /* 1KHz */
	INV_ICM20602_GYRO_LFP_92HZ,
	INV_ICM20602_GYRO_LFP_41HZ,
	INV_ICM20602_GYRO_LFP_20HZ,
	INV_ICM20602_GYRO_LFP_10HZ,
	INV_ICM20602_GYRO_LFP_5HZ,
	INV_ICM20602_GYRO_LFP_NUM,
};

enum inv_icm20602_gyro_sample_rate_e {
	ICM20602_SAMPLE_RATE_100HZ = 100,
	ICM20602_SAMPLE_RATE_200HZ = 200,
	ICM20602_SAMPLE_RATE_500HZ = 500,
	ICM20602_SAMPLE_RATE_1000HZ = 1000
};

/* this is for ACCEL CONFIGURATION 2 reg */
enum inv_icm20602_acc_lpf_e {
	ICM20602_ACCLFP_218 = 1,
	ICM20602_ACCLFP_99,
	ICM20602_ACCLFP_44,
	ICM20602_ACCLFP_21,
	ICM20602_ACCLFP_10,
	ICM20602_ACCLFP_5,
	ICM20602_ACCLFP_420_NOLPF,
	ICM20602_ACCLPF_NUM = 7
};

/* IIO attribute address */
enum INV_ICM20602_IIO_ATTR_ADDR {
	ATTR_ICM20602_GYRO_MATRIX,
	ATTR_ICM20602_ACCL_MATRIX,
};

/* this is for GYRO CONFIGURATION reg */
enum inv_icm20602_fs_e {
	INV_ICM20602_FS_250DPS = 0,
	INV_ICM20602_FS_500DPS,
	INV_ICM20602_FS_1000DPS,
	INV_ICM20602_FS_2000DPS,
	NUM_ICM20602_FS
};

enum inv_icm20602_clock_sel_e {
	INV_ICM20602_CLK_INTERNAL = 0,
	INV_ICM20602_CLK_PLL,
	INV_NUM_CLK
};

enum inv_icm20602_spi_freq {
	MPU_SPI_FREQUENCY_1MHZ = 960000UL,
	MPU_SPI_FREQUENCY_5MHZ = 4800000UL,
	MPU_SPI_FREQUENCY_8MHZ = 8000000UL,
	MPU_SPI_FREQUENCY_10MHZ = 10000000UL,
	MPU_SPI_FREQUENCY_15MHZ = 15000000UL,
	MPU_SPI_FREQUENCY_20MHZ = 20000000UL,
};

#define MPU_SPI_BUF_LEN   512
#define	ICM20602_DEV_NAME	"icm20602_iio"

struct inv_icm20602_platform_data {
	__s8 orientation[9];
};

struct X_Y_Z {
	u32 X;
	u32 Y;
	u32 Z;
};

enum RAW_TYPE {
	ACCEL = 1,
	GYRO  = 2,
	TEMP  = 4,
};

struct icm20602_user_config {
	enum inv_icm20602_gyro_temp_lpf_e gyro_lpf;
	enum inv_icm20602_gyro_fsr_e gyro_fsr;
	struct X_Y_Z gyro_self_test;

	enum inv_icm20602_acc_lpf_e	acc_lpf;
	enum inv_icm20602_acc_fsr_e	acc_fsr;
	struct X_Y_Z acc_self_test;

	uint32_t gyro_accel_sample_rate;
	uint32_t user_fps_in_ms;

	bool fifo_enabled;
	uint32_t fifo_waterlevel;
	struct X_Y_Z wake_on_motion;
};

enum inv_icm20602_interface {
	ICM20602_I2C = 0,
	ICM20602_SPI
};

/*
 *  struct inv_icm20602_state - Driver state variables.
 *  @TIMESTAMP_FIFO_SIZE: fifo size for timestamp.
 *  @trig:              IIO trigger.
 *  @chip_config:	Cached attribute information.
 *  @reg:		Map of important registers.
 *  @hw:		Other hardware-specific information.
 *  @chip_type:		chip type.
 *  @time_stamp_lock:	spin lock to time stamp.
 *  @spi: spi devices
 *  @plat_data:		platform data.
 *  @timestamps:        kfifo queue to store time stamp.
 */
struct inv_icm20602_state {
#define TIMESTAMP_FIFO_SIZE 32
	enum inv_icm20602_interface interface;
	struct iio_trigger  *trig;
	const struct inv_icm20602_reg_map *reg;
	struct icm20602_user_config *config;
	spinlock_t time_stamp_lock;
	struct spi_device *spi;
	struct i2c_client *client;
	u8 fifo_packet_size;
	int fifo_cnt_threshold;
	char *buf;
	struct struct_icm20602_data *data_push;
	enum inv_devices chip_type;
	int gpio;
	DECLARE_KFIFO(timestamps, long long, TIMESTAMP_FIFO_SIZE);
};

struct struct_icm20602_raw_data {
	u8 ACCEL_XOUT_H;
	u8 ACCEL_XOUT_L;

	u8 ACCEL_YOUT_H;
	u8 ACCEL_YOUT_L;

	u8 ACCEL_ZOUT_H;
	u8 ACCEL_ZOUT_L;

	u8 TEMP_OUT_H;
	u8 TEMP_OUT_L;

	u8 GYRO_XOUT_H;
	u8 GYRO_XOUT_L;

	u8 GYRO_YOUT_H;
	u8 GYRO_YOUT_L;

	u8 GYRO_ZOUT_H;
	u8 GYRO_ZOUT_L;
};

struct struct_icm20602_real_data {
	u16 ACCEL_XOUT;
	u16 ACCEL_YOUT;
	u16 ACCEL_ZOUT;

	u16 GYRO_XOUT;
	u16 GYRO_YOUT;
	u16 GYRO_ZOUT;

	u16 TEMP_OUT;
};

struct struct_icm20602_data {
	s64 timestamps;
	struct struct_icm20602_raw_data raw_data;
	struct struct_icm20602_real_data real_data;
};

extern struct iio_trigger *inv_trig;
irqreturn_t inv_icm20602_irq_handler(int irq, void *p);
irqreturn_t inv_icm20602_read_fifo_fn(int irq, void *p);
int inv_icm20602_reset_fifo(struct iio_dev *indio_dev);

int inv_icm20602_probe_trigger(struct iio_dev *indio_dev);
void inv_icm20602_remove_trigger(struct inv_icm20602_state *st);
int inv_icm20602_validate_trigger(struct iio_dev *indio_dev,
				struct iio_trigger *trig);

int icm20602_read_raw(struct inv_icm20602_state *st,
		struct struct_icm20602_real_data *real_data, u8 type);

int icm20602_init_reg_map(void);
int icm20602_init_device(struct inv_icm20602_state *st);
int icm20602_detect(struct inv_icm20602_state *st);
int icm20602_read_fifo(struct inv_icm20602_state *st,
	void *buf, const int size);
int icm20602_start_fifo(struct inv_icm20602_state *st);
#endif
