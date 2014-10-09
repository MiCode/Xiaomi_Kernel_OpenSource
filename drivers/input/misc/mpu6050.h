/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

/*register and associated bit definition*/

#ifndef __MPU6050_H__
#define __MPU6050_H__

#define REG_SAMPLE_RATE_DIV	0x19
#define REG_CONFIG		0x1A

#define REG_GYRO_CONFIG		0x1B
#define BITS_SELF_TEST_EN	0xE0
#define GYRO_CONFIG_FSR_SHIFT	3

#define REG_ACCEL_CONFIG	0x1C
#define REG_ACCEL_MOT_THR	0x1F
#define REG_ACCEL_MOT_DUR	0x20
#define ACCL_CONFIG_FSR_SHIFT	3

#define REG_ACCELMOT_THR	0x1F

#define REG_ACCEL_MOT_DUR	0x20

#define REG_FIFO_EN		0x23
#define BIT_ACCEL_OUT		0x08
#define BITS_GYRO_OUT		0x70

#define REG_INT_PIN_CFG		0x37
#define BIT_INT_ACTIVE_LOW	0x80
#define BIT_INT_OPEN_DRAIN	0x40
#define BIT_INT_LATCH_EN	0x20
#define BIT_INT_RD_CLR		0x10
#define BIT_I2C_BYPASS_EN	0x02
#define BIT_INT_CFG_DEFAULT	(BIT_INT_LATCH_EN | BIT_INT_RD_CLR)

#define REG_INT_ENABLE		0x38
#define BIT_DATA_RDY_EN		0x01
#define BIT_DMP_INT_EN		0x02
#define BIT_ZMOT_EN		0x20
#define BIT_MOT_EN		0x40
#define BIT_6500_WOM_EN		0x40

#define REG_DMP_INT_STATUS	0x39
#define REG_INT_STATUS		0x3A
#define BIT_DATA_RDY_INT	0x01
#define BIT_DMP_INT_INT		0x02
#define BIT_ZMOT_INT		0x20
#define BIT_MOT_INT		0x40
#define BIT_6500_WOM_INT	0x40

#define REG_RAW_ACCEL		0x3B
#define REG_TEMPERATURE		0x41
#define REG_RAW_GYRO		0x43
#define REG_EXT_SENS_DATA_00	0x49

#define BIT_FIFO_RST		0x04
#define BIT_DMP_RST		0x08
#define BIT_I2C_MST_EN		0x20
#define BIT_FIFO_EN		0x40
#define BIT_DMP_EN		0x80
#define BIT_ACCEL_FIFO		0x08
#define BIT_GYRO_FIFO		0x70

#define REG_DETECT_CTRL		0x69
#define MOT_DET_DELAY_SHIFT	4

#define REG_PWR_MGMT_1		0x6B
#define BIT_H_RESET		0x80
#define BIT_SLEEP		0x40
#define BIT_CYCLE		0x20
#define BIT_CLK_MASK		0x07
#define BIT_RESET_ALL		0xCF
#define BIT_WAKEUP_AFTER_RESET	0x00

#define REG_PWR_MGMT_2		0x6C
#define BIT_PWR_ACCEL_STBY_MASK	0x38
#define BIT_PWR_GYRO_STBY_MASK	0x07
#define BIT_LPA_FREQ_MASK	0xC0
#define BITS_PWR_ALL_AXIS_STBY	(BIT_PWR_ACCEL_STBY_MASK |\
				BIT_PWR_GYRO_STBY_MASK)

#define REG_FIFO_COUNT_H	0x72
#define REG_FIFO_R_W		0x74
#define REG_WHOAMI		0x75

#define SAMPLE_DIV_MAX		0xFF
#define ODR_DLPF_DIS		8000
#define ODR_DLPF_ENA		1000

/* Min delay = MSEC_PER_SEC/ODR_DLPF_ENA */
/* Max delay = MSEC_PER_SEC/(ODR_DLPF_ENA/SAMPLE_DIV_MAX+1) */
#define DELAY_MS_MIN_DLPF	1
#define DELAY_MS_MAX_DLPF	256

/* Min delay = MSEC_PER_SEC/ODR_DLPF_DIS and round up to 1*/
/* Max delay = MSEC_PER_SEC/(ODR_DLPF_DIS/SAMPLE_DIV_MAX+1) */
#define DELAY_MS_MIN_NODLPF	1
#define DELAY_MS_MAX_NODLPF	32

/* device bootup time in millisecond */
#define POWER_UP_TIME_MS	100
/* delay to wait gyro engine stable in millisecond */
#define SENSOR_UP_TIME_MS	30
/* delay between power operation in millisecond */
#define POWER_EN_DELAY_US	10

#define MPU6050_LPA_5HZ		0x40

/* initial configure */
#define INIT_FIFO_RATE		200
#define DEFAULT_MOT_THR		1
#define DEFAULT_MOT_DET_DUR	1
#define DEFAULT_MOT_DET_DELAY	0

/* chip reset wait */
#define MPU6050_RESET_RETRY_CNT	10
#define MPU6050_RESET_WAIT_MS	20

enum mpu_device_id {
	MPU6050_ID = 0x68,
	MPU6500_ID = 0x70,
};

enum mpu_fsr {
	MPU_FSR_250DPS = 0,
	MPU_FSR_500DPS,
	MPU_FSR_1000DPS,
	MPU_FSR_2000DPS,
	NUM_FSR
};

enum mpu_filter {
	MPU_DLPF_256HZ_NOLPF2 = 0,
	MPU_DLPF_188HZ,
	MPU_DLPF_98HZ,
	MPU_DLPF_42HZ,
	MPU_DLPF_20HZ,
	MPU_DLPF_10HZ,
	MPU_DLPF_5HZ,
	MPU_DLPF_RESERVED,
	NUM_FILTER
};

enum mpu_clock_source {
	MPU_CLK_INTERNAL = 0,
	MPU_CLK_PLL_X,
	NUM_CLK
};

enum mpu_accl_fs {
	ACCEL_FS_02G = 0,
	ACCEL_FS_04G,
	ACCEL_FS_08G,
	ACCEL_FS_16G,
	NUM_ACCL_FSR
};

/* Sensitivity Scale Factor */
/* Sensor HAL will take 1024 LSB/g */
enum mpu_accel_fs_shift {
	ACCEL_SCALE_SHIFT_02G = 4,
	ACCEL_SCALE_SHIFT_04G = 3,
	ACCEL_SCALE_SHIFT_08G = 2,
	ACCEL_SCALE_SHIFT_16G = 1
};

enum mpu_gyro_fs_shift {
	GYRO_SCALE_SHIFT_FS0 = 3,
	GYRO_SCALE_SHIFT_FS1 = 2,
	GYRO_SCALE_SHIFT_FS2 = 1,
	GYRO_SCALE_SHIFT_FS3 = 0
};

/*device enum */
enum inv_devices {
	INV_MPU6050,
	INV_MPU6500,
	INV_MPU6XXX,
	INV_NUM_PARTS
};

/**
 *  struct mpu_reg_map_s - Notable slave registers.
 *  @sample_rate_div:	Divider applied to gyro output rate.
 *  @lpf:		Configures internal LPF.
 *  @fifo_en:	Determines which data will appear in FIFO.
 *  @gyro_config:	gyro config register.
 *  @accel_config:	accel config register
 *  @mot_thr:	Motion detection threshold.
 *  @fifo_count_h:	Upper byte of FIFO count.
 *  @fifo_r_w:	FIFO register.
 *  @raw_gyro:	Address of first gyro register.
 *  @raw_accl:	Address of first accel register.
 *  @temperature:	temperature register.
 *  @int_pin_cfg:	Interrupt pin and I2C bypass configuration.
 *  @int_enable:	Interrupt enable register.
 *  @int_status:	Interrupt flags.
 *  @pwr_mgmt_1:	Controls chip's power state and clock source.
 *  @pwr_mgmt_2:	Controls power state of individual sensors.
 */
struct mpu_reg_map {
	u8 sample_rate_div;
	u8 lpf;
	u8 user_ctrl;
	u8 fifo_en;
	u8 gyro_config;
	u8 accel_config;
	u8 fifo_count_h;
	u8 mot_thr;
	u8 mot_dur;
	u8 fifo_r_w;
	u8 raw_gyro;
	u8 raw_accel;
	u8 temperature;
	u8 int_pin_cfg;
	u8 int_enable;
	u8 int_status;
	u8 pwr_mgmt_1;
	u8 pwr_mgmt_2;
};

/**
 *  struct mpu_chip_config - Cached chip configuration data.
 *  @fsr:		Full scale range.
 *  @lpf:		Digital low pass filter frequency.
 *  @accl_fs:		accel full scale range.
 *  @enable:		master enable to enable output
 *  @accel_enable:	enable accel functionality
 *  @accel_fifo_enable:	enable accel data output
 *  @gyro_enable:	enable gyro functionality
 *  @gyro_fifo_enable:	enable gyro data output
 *  @is_asleep:		1 if chip is powered down.
 *  @lpa_mod:		low power mode.
 *  @tap_on:		tap on/off.
 *  @flick_int_on:		flick interrupt on/off.
 *  @int_enabled:		interrupt is enabled.
 *  @lpa_freq:		low power frequency
 *  @rate_div:		Sampling rate divider.
 */
struct mpu_chip_config {
	u32 fsr:2;
	u32 lpf:3;
	u32 accel_fs:2;
	u32 enable:1;
	u32 accel_enable:1;
	u32 accel_fifo_enable:1;
	u32 gyro_enable:1;
	u32 gyro_fifo_enable:1;
	u32 is_asleep:1;
	u32 lpa_mode:1;
	u32 tap_on:1;
	u32 flick_int_on:1;
	u32 int_enabled:1;
	u32 mot_det_on:1;
	u8 int_pin_cfg;
	u16 lpa_freq;
	u16 rate_div;
};


/**
 *  struct mpu6050_platform_data - device platform dependent data.
 *  @gpio_en:		enable GPIO.
 *  @gpio_int:		interrupt GPIO.
 *  @int_flags:		interrupt pin control flags.
 *  @use_int:		use interrupt mode instead of polling data.
 *  @place:			sensor place number.
 */
struct mpu6050_platform_data {
	int gpio_en;
	int gpio_int;
	u32 int_flags;
	bool use_int;
	u8 place;
};

#endif /* __MPU6050_H__ */
