/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* Code was copied and modified from
* drivers/iio/imu/inv_mpu6050/inv_mpu_iio.h
*
* Copyright (C) 2012 Invensense, Inc.
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

#ifndef _INV_ICM20689_IIO_H_
#define _INV_ICM20689_IIO_H_

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

/***
 *   it uses sensor irq to trigger
 *   if set INV20689_DEVICE_IRQ_TRIGGER as 1,
 *   otherwise use SMD IRQ to trigger
 */
#define INV20689_DEVICE_IRQ_TRIGGER    0

#if INV20689_DEVICE_IRQ_TRIGGER
#define INV20689_SMD_IRQ_TRIGGER    0
#else
#define INV20689_SMD_IRQ_TRIGGER    1
#endif

/**
 *  struct inv_icm20689_reg_map - Notable registers.
 *  @sample_rate_div:	Divider applied to gyro output rate.
 *  @lpf:		Configures internal low pass filter.
 *  @user_ctrl:		Enables/resets the FIFO.
 *  @fifo_en:		Determines which data will appear in FIFO.
 *  @gyro_config:	gyro config register.
 *  @accl_config:	accel config register
 *  @fifo_count_h:	Upper byte of FIFO count.
 *  @fifo_r_w:		FIFO register.
 *  @raw_gyro:		Address of first gyro register.
 *  @raw_accl:		Address of first accel register.
 *  @temperature:	temperature register
 *  @int_enable:	Interrupt enable register.
 *  @pwr_mgmt_1:	Controls chip's power state and clock source.
 *  @pwr_mgmt_2:	Controls power state of individual sensors.
 */
struct inv_icm20689_reg_map {
	u8 sample_rate_div;
	u8 config;
	u8 user_ctrl;
	u8 fifo_en;
	u8 gyro_config;
	u8 accl_config;
	u8 accl_config2;
	u8 fifo_count_h;
	u8 fifo_r_w;
	u8 raw_gyro;
	u8 raw_accl;
	u8 temperature;
	u8 int_enable;
	u8 pwr_mgmt_1;
	u8 pwr_mgmt_2;
	u8 whoami;
};

/*device enum */
enum inv_devices {
	INV_ICM20689,
	INV_NUM_PARTS
};


/*register and associated bit definition*/
#define INV_ICM20689_REG_SAMPLE_RATE_DIV     0x19
#define INV_ICM20689_REG_CONFIG              0x1A
#define INV_ICM20689_REG_GYRO_CONFIG         0x1B
#define INV_ICM20689_REG_ACCEL_CONFIG	    0x1C
#define INV_ICM20689_REG_ACCEL_CONFIG2	    0x1D


#define INV_ICM20689_REG_FIFO_EN             0x23
#define INV_ICM20689_BIT_ACCEL_OUT                   0x08
#define INV_ICM20689_BITS_GYRO_OUT                   0x70
#define INV_ICM20689_BITS_TEMP_OUT                   0x80

#define INV_ICM20689_REG_INT_BYPASS          0x37

#define INV_ICM20689_REG_INT_ENABLE          0x38
#define INV_ICM20689_BIT_DATA_RDY_EN                 0x01
#define INV_ICM20689_BIT_DMP_INT_EN                  0x02

#define INV_ICM20689_REG_RAW_ACCEL           0x3B
#define INV_ICM20689_REG_TEMPERATURE         0x41
#define INV_ICM20689_REG_RAW_GYRO            0x43

#define INV_ICM20689_REG_USER_CTRL           0x6A
#define INV_ICM20689_BIT_FIFO_RST                    0x04
#define INV_ICM20689_BIT_I2C_MST_DIS                  0x10
#define INV_ICM20689_BIT_FIFO_EN                     0x40

#define INV_ICM20689_REG_PWR_MGMT_1          0x6B
#define INV_ICM20689_BIT_H_RESET                     0x80
#define INV_ICM20689_BIT_SLEEP                       0x40
#define INV_ICM20689_BIT_CLK_MASK                    0x7

#define INV_ICM20689_REG_PWR_MGMT_2          0x6C
#define INV_ICM20689_BIT_PWR_ACCL_STBY               0x38
#define INV_ICM20689_BIT_PWR_GYRO_STBY               0x07

#define INV_ICM20689_REG_FIFO_COUNT_H        0x72
#define INV_ICM20689_REG_FIFO_R_W            0x74

#define INV_ICM20689_REG_WHOAMI			0x75

#define INV_ICM20689_BYTES_PER_3AXIS_SENSOR   6
#define INV_ICM20689_FIFO_COUNT_BYTE          2
#define INV_ICM20689_FIFO_THRESHOLD           500
#define INV_ICM20689_POWER_UP_TIME            100
#define INV_ICM20689_TEMP_UP_TIME             100
#define INV_ICM20689_SENSOR_UP_TIME           20

#define INV_ICM20689_TEMP_OFFSET	          12421
#define INV_ICM20689_TEMP_SCALE               2941
#define INV_ICM20689_MAX_GYRO_FS_PARAM        3
#define INV_ICM20689_MAX_ACCL_FS_PARAM        3
#define INV_ICM20689_THREE_AXIS               3
#define INV_ICM20689_GYRO_CONFIG_FSR_SHIFT    3
#define INV_ICM20689_ACCL_CONFIG_FSR_SHIFT    3

/* 6 + 6 round up and plus 8 */
#define INV_ICM20689_OUTPUT_DATA_SIZE         24
#define INV_ICM20689_MAX_FIFO_OUTPUT          256
#define INV_ICM20689_STORED_DATA              512

/* init parameters */
#define INV_ICM20689_INIT_FIFO_RATE           50  /* data rdy rate */
#define INV_ICM20689_MAX_FIFO_RATE            1000
#define INV_ICM20689_MIN_FIFO_RATE            4
#define INV_ICM20689_GYRO_8K_RATE             8000

#define INV_ICM20689_TIME_STAMP_TOR           5
#define INV_ICM20689_ONE_K_HZ                 1000

/* scan element definition */
enum inv_icm20689_scan {
	INV_ICM20689_SCAN_ACCL_X,
	INV_ICM20689_SCAN_ACCL_Y,
	INV_ICM20689_SCAN_ACCL_Z,
	INV_ICM20689_SCAN_GYRO_X,
	INV_ICM20689_SCAN_GYRO_Y,
	INV_ICM20689_SCAN_GYRO_Z,
	INV_ICM20689_SCAN_TEMP,
	INV_ICM20689_SCAN_TIMESTAMP,
};

/* this is for CONFIGURATION reg bit[2:0] DLFP_CFG */
enum inv_icm20689_gyro_lpf_e {
	INV_ICM20689_GYRO_LFP_250HZ = 0,  /* 8KHz */
	INV_ICM20689_GYRO_LFP_176HZ,      /* 1KHz */
	INV_ICM20689_GYRO_LFP_92HZ,
	INV_ICM20689_GYRO_LFP_41HZ,
	INV_ICM20689_GYRO_LFP_20HZ,
	INV_ICM20689_GYRO_LFP_10HZ,
	INV_ICM20689_GYRO_LFP_5HZ,
	INV_ICM20689_GYRO_LFP_3200HZ_NOLPF, /* 8KHz */
	NUM_ICM20689_GYRO_LFP
};

/* this is for ACCEL CONFIGURATION 2 reg */
enum inv_icm20689_acc_lpf_e {
	INV_ICM20689_ACC_LFP_218HZ = 0,
	INV_ICM20689_ACC_LFP_217HZ,
	INV_ICM20689_ACC_LFP_99HZ,
	INV_ICM20689_ACC_LFP_44HZ,
	INV_ICM20689_ACC_LFP_21HZ,
	INV_ICM20689_ACC_LFP_10HZ,
	INV_ICM20689_ACC_LFP_5HZ,
	INV_ICM20689_ACC_LFP_420HZ_NOLPF,
	NUM_ICM20689_ACC_LFP
};

/* IIO attribute address */
enum INV_ICM20689_IIO_ATTR_ADDR {
	ATTR_ICM20689_GYRO_MATRIX,
	ATTR_ICM20689_ACCL_MATRIX,
};

/* this is for ACCEL CONFIGURATION reg */
enum inv_icm20689_accl_fs_e {
	INV_ICM20689_FS_02G = 0,
	INV_ICM20689_FS_04G,
	INV_ICM20689_FS_08G,
	INV_ICM20689_FS_16G,
	NUM_ACCL_FSR
};

/* this is for GYRO CONFIGURATION reg */
enum inv_icm20689_fs_e {
	INV_ICM20689_FS_250DPS = 0,
	INV_ICM20689_FS_500DPS,
	INV_ICM20689_FS_1000DPS,
	INV_ICM20689_FS_2000DPS,
	NUM_ICM20689_FS
};

enum inv_icm20689_clock_sel_e {
	INV_ICM20689_CLK_INTERNAL = 0,
	INV_ICM20689_CLK_PLL,
	INV_NUM_CLK
};

enum inv_icm20689_spi_freq {
	MPU_SPI_FREQUENCY_1MHZ = 960000UL,
	MPU_SPI_FREQUENCY_5MHZ = 4800000UL,
	MPU_SPI_FREQUENCY_8MHZ = 8000000UL,
	MPU_SPI_FREQUENCY_10MHZ = 10000000UL,
	MPU_SPI_FREQUENCY_15MHZ = 15000000UL,
	MPU_SPI_FREQUENCY_20MHZ = 20000000UL,
};

#define MPU_SPI_BUF_LEN   512
#define	ICM20689_DEV_NAME	"icm20689_iio"

struct inv_icm20689_chip_config {
	enum inv_icm20689_gyro_lpf_e            gyro_lpf;
	enum inv_icm20689_acc_lpf_e             acc_lpf;
	enum inv_icm20689_fs_e                  gyro_fsr;
	enum inv_icm20689_accl_fs_e             acc_fsr;
	unsigned int enable:1;
	unsigned int accl_fifo_enable:1;
	unsigned int gyro_fifo_enable:1;
	unsigned int temp_fifo_enable:1;
	u8	fifo_mask;
	u16 fifo_rate;
};

/**
 *  struct inv_icm20689_hw - Other important hardware information.
 *  @num_reg:	Number of registers on device.
 *  @name:      name of the chip.
 *  @reg:   register map of the chip.
 *  @config:    configuration of the chip.
 */
struct inv_icm20689_hw {
	u8 num_reg;
	u8 *name;
	const struct inv_icm20689_reg_map *reg;
	struct inv_icm20689_chip_config *config;
};

/*
 *  struct inv_icm20689_state - Driver state variables.
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
struct inv_icm20689_state {
#define TIMESTAMP_FIFO_SIZE 32
	struct iio_trigger  *trig;
	struct inv_icm20689_chip_config chip_config;
	const struct inv_icm20689_reg_map *reg;
	const struct inv_icm20689_hw *hw;
	enum   inv_devices chip_type;
#if INV20689_DEVICE_IRQ_TRIGGER
	int gpio;
#endif
	spinlock_t time_stamp_lock;
	struct spi_device *spi;
	struct inv_mpu6050_platform_data plat_data;
	uint8_t *tx_buf;   /* used for spi transaction */
	uint8_t *rx_buf;   /* used for spi transaction */
	DECLARE_KFIFO(timestamps, long long, TIMESTAMP_FIFO_SIZE);
};

extern struct iio_trigger *inv_trig;
irqreturn_t inv_icm20689_irq_handler(int irq, void *p);
irqreturn_t inv_icm20689_read_fifo_fn(int irq, void *p);
int inv_icm20689_probe_trigger(struct iio_dev *indio_dev);
void inv_icm20689_remove_trigger(struct inv_icm20689_state *st);
int inv_icm20689_reset_fifo(struct iio_dev *indio_dev);
int inv_icm20689_switch_engine(struct inv_icm20689_state *st,
		bool en, u32 mask);
int inv_icm20689_write_reg(struct inv_icm20689_state *st,
		int reg, u8 val);
int inv_icm20689_read_reg(struct inv_icm20689_state *st,
		uint8_t reg, uint8_t *val);
int inv_icm20689_set_power_itg(struct inv_icm20689_state *st,
		bool power_on);
extern int inv_icm20689_spi_bulk_read(struct inv_icm20689_state *st,
		int reg, uint8_t length, uint8_t *buf);
extern int imu_ts_smd_channel_init(void);
extern void imu_ts_smd_channel_close(void);

#endif
