/*
* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _INV_MPU9250_IIO_H_
#define _INV_MPU9250_IIO_H_

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

extern int mpu9250_debug_enable;

#define dev_dbginfo(fmt, ...) \
	do { \
		if (mpu9250_debug_enable > 0) { \
			printk(fmt, ##__VA_ARGS__);	  \
		} \
	} while (0)

#define dev_dbgerr(fmt, ...) printk(fmt, ##__VA_ARGS__)

/**
* debugfs bits Enum
*/
enum _bits {
	UNSET_DEBUG = 0,
	SET_DETECT  = 1,
	READ_FIFO   = 2,
	BULK_READ_RAW = 3,
	SET_DEBUG   = 0xff,
};

enum _mpu_err {
	MPU_SUCCESS = 0,
	MPU_FAIL = 1,
	MPU_READ_FAIL = 2,
	MPU_WRITE_FAIL = 3,
};

enum init_config {
	INIT_200HZ = 0,
	INIT_1000HZ = 1,
	INIT_8000HZ = 2,
	INIT_NULL,
};


/**
* Gyro Low Pass Filter Enum
* MPU9250_GYRO_LPF_250HZ and MPU9250_GYRO_LPF_3600HZ_NOLPF is only applicable
* for 8KHz sample rate. All other LPF values are only applicable for 1KHz
* internal sample rate.
*/
enum gyro_lpf_e {
	MPU9250_GYRO_LPF_250HZ = 0,
	MPU9250_GYRO_LPF_184HZ,
	MPU9250_GYRO_LPF_92HZ,
	MPU9250_GYRO_LPF_41HZ,
	MPU9250_GYRO_LPF_20HZ,
	MPU9250_GYRO_LPF_10HZ,
	MPU9250_GYRO_LPF_5HZ,
	MPU9250_GYRO_LPF_3600HZ_NOLPF,
	NUM_MPU9250_GYRO_LPF
};

/**
*Accelerometer Low Pass Filter Enum.
*/
enum acc_lpf_e {
	MPU9250_ACC_LPF_460HZ = 0,
	MPU9250_ACC_LPF_184HZ,
	MPU9250_ACC_LPF_92HZ,
	MPU9250_ACC_LPF_41HZ,
	MPU9250_ACC_LPF_20HZ,
	MPU9250_ACC_LPF_10HZ,
	MPU9250_ACC_LPF_5HZ,
	MPU9250_ACC_LPF_460HZ_NOLPF,
	NUM_MPU9250_ACC_LPF
};

/**
* Gyro Full Scale Range Enum
*/
enum gyro_fsr_e {
	MPU9250_GYRO_FSR_250DPS = 0,
	MPU9250_GYRO_FSR_500DPS,
	MPU9250_GYRO_FSR_1000DPS,
	MPU9250_GYRO_FSR_2000DPS,
	NUM_MPU9250_GYRO_FSR
};

/**
* Accelerometor Full Scale Range Enum
*/
enum acc_fsr_e {
	MPU9250_ACC_FSR_2G = 0,
	MPU9250_ACC_FSR_4G,
	MPU9250_ACC_FSR_8G,
	MPU9250_ACC_FSR_16G,
	NUM_MPU9250_ACC_FSR
};

/**
* Supported Sample rate for gyro.
* If gyro sample rate is set to 8KHz, accelerometer sample rate is 1KHz.
* If other sample rate is selected, the same sample rate is set for gyro and
* accelerometer. */
enum gyro_sample_rate_e {
	MPU9250_SAMPLE_RATE_100HZ = 0,
	MPU9250_SAMPLE_RATE_200HZ,
	MPU9250_SAMPLE_RATE_500HZ,
	MPU9250_SAMPLE_RATE_1000HZ,
	MPU9250_SAMPLE_RATE_8000HZ,
	NUM_MPU9250_SAMPLE_RATE
};

#define MPU9250_COMPASS_MAX_SAMPLE_RATE_HZ   100
/**
* Sample rate for compass.
* NOTE: only 100Hz compass sampling rate is supported in current driver.
*/
enum compass_sample_rate_e {
	MPU9250_COMPASS_SAMPLE_RATE_100HZ = 0,
	NUM_MPU9250_COMPASS_SAMPLE_RATE
};

/**
* MPU9250 Register Addresses.
* Here defines only the register addresses used in mpu9250 driver.
* See the spec for the full register list.
*/
enum MPU9250_REG_ADDR {
	MPU9250_REG_SMPLRT_DIV     = 25,
	MPU9250_REG_CONFIG         = 26,
	MPU9250_REG_GYRO_CONFIG    = 27,
	MPU9250_REG_ACCEL_CONFIG   = 28,
	MPU9250_REG_ACCEL_CONFIG2  = 29,
	MPU9250_REG_LP_ACCEL_ODR   = 30,
	MPU9250_REG_FIFO_ENABLE    = 35,
	MPU9250_REG_I2C_MST_CTRL   = 36,
	MPU9250_REG_I2C_SLV0_ADDR  = 37,
	MPU9250_REG_I2C_SLV0_REG   = 38,
	MPU9250_REG_I2C_SLV0_CTRL  = 39,
	MPU9250_REG_I2C_SLV1_ADDR  = 40,
	MPU9250_REG_I2C_SLV1_REG   = 41,
	MPU9250_REG_I2C_SLV1_CTRL  = 42,
	MPU9250_REG_I2C_SLV4_ADDR  = 49,
	MPU9250_REG_I2C_SLV4_REG   = 50,
	MPU9250_REG_I2C_SLV4_DO    = 51,
	MPU9250_REG_I2C_SLV4_CTRL  = 52,
	MPU9250_REG_I2C_SLV4_DI    = 53,
	MPU9250_REG_I2C_MST_STATUS = 54,
	MPU9250_REG_INT_BYPASS     = 55,
	MPU9250_REG_INT_EN         = 56,
	MPU9250_REG_INT_STATUS     = 58,
	MPU9250_REG_I2C_SLV1_DO    = 100,
	MPU9250_REG_I2C_MST_DELAY_CTRL = 103,
	MPU9250_REG_USER_CTRL      = 106,
	MPU9250_REG_PWR_MGMT1      = 107,
	MPU9250_REG_PWR_MGMT2      = 108,
	MPU9250_REG_FIFO_COUNTH    = 114,
	MPU9250_REG_FIFO_COUNTL    = 115,
	MPU9250_REG_FIFO_RW        = 116,
	MPU9250_REG_WHOAMI         = 117,
};

/**
* MPU9250 Compass Register Addresses.
* Here defines only the register addresses used in mpu9250 driver.
* See the spec for the full register list.
*/
enum MPU9250_COMPASS_REG_ADDR {
	MPU9250_COMP_REG_WIA       = 0x00,
	MPU9250_COMP_REG_CNTL1     = 0x0a,
	MPU9250_COMP_REG_ASAX      = 0x10,
	MPU9250_COMP_REG_ASAY      = 0x11,
	MPU9250_COMP_REG_ASAZ      = 0x12,
};

/*bit ops*/
#define BIT_FIFO_SIZE_1024  (0x40)
#define BIT_FIFO_SIZE_2048  (0x80)
#define BIT_FIFO_SIZE_4096  (0xC0)
#define BIT_I2C_IF_DIS      (0x10)
#define BIT_FIFO_RST        (0x04)
#define BIT_I2C_MST_RST     (0x02)
#define BIT_DMP_RST         (0x08)
#define BIT_RAW_RDY_EN      (0x01)
#define BIT_WAIT_FOR_ES     (0x40)
#define BIT_I2C_MST_EN      (0x20)
#define BIT_DELAY_ES_SHADOW (0x80)
#define BIT_SLV4_DLY_EN     (0x10)
#define BIT_SLV3_DLY_EN     (0x08)
#define BIT_SLV2_DLY_EN     (0x04)
#define BIT_SLV1_DLY_EN     (0x02)
#define BIT_SLV0_DLY_EN     (0x01)
#define BIT_FIFO_EN         (0x40)
#define BIT_TEMP_FIFO_EN    (0x80)
#define BIT_GYRO_FIFO_EN    (0x70)
#define BIT_ACCEL_FIFO_EN   (0x08)
#define BIT_FIFO_OVERFLOW_INT  (0x10)
#define BIT_H_RESET         (0x80)
#define BIT_SLEEP           (0X40)

#define MPU9250_INTERNAL_SAMPLE_RATE_HZ    1000
#define WHO_AM_I_REG_VAL 0x71

/**
* Full Scale Range of the magnetometer chip AK89xx in MPU9250
*/
#define MPU9250_AK89xx_FSR  4915
#define MPU9250_AKM_DEV_ID  0x48

struct mpu9250_context {
	int gyro_lpf;
	int accel_lpf;
	int gyro_sample_rate;
	int accel_sample_rate;
	int compass_sample_rate;
	int gyro_fsr;
	int accel_fsr;
	int compass_fsr;
};

/**
* Cached register configurations
* This is to keep track of all the registers which have been set
*/
struct reg_cfg {
	uint8_t smplrt_div;
	uint8_t config;
	uint8_t gyro_config;
	uint8_t accel_config;
	uint8_t accel_config2;
	uint8_t fifo_enable;
	uint8_t i2c_mst_ctrl;
	uint8_t int_cfg;
	uint8_t int_en;
	uint8_t i2c_mst_delay_ctrl;
	uint8_t user_ctrl;
	uint8_t pwr_mgmt1;
	uint8_t pwr_mgmt2;
	uint8_t fifo_counth;
	uint8_t fifo_countl;
	uint8_t init_config;
};

/**
 *  struct inv_mpu9250_reg_map - Notable registers.
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
struct inv_mpu9250_reg_map {
	u8 sample_rate_div;
	u8 config;
	u8 user_ctrl;
	u8 fifo_en;
	u8 int_cfg;
	u8 gyro_config;
	u8 accl_config;
	u8 accl_config2;
	u8 fifo_count_h;
	u8 fifo_count_l;
	u8 i2c_mst_ctrl;
	u8 i2c_slv0_addr;
	u8 i2c_slv0_reg;
	u8 i2c_slv0_ctrl;
	u8 i2c_slv1_addr;
	u8 i2c_slv1_reg;
	u8 i2c_slv1_ctrl;
	u8 i2c_slv1_do;
	u8 i2c_slv4_addr;
	u8 i2c_slv4_reg;
	u8 i2c_slv4_ctrl;
	u8 i2c_slv4_di;
	u8 i2c_slv4_do;
	u8 i2c_mst_status;
	u8 i2c_mst_delay_ctrl;
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
	INV_MPU9250,
	INV_NUM_PARTS
};

/***
 *   it uses sensor irq to trigger
 *   if set MPU9250_DEVICE_IRQ_TRIGGER as 1,
 *   otherwise use SMD IRQ to trigger
 */
#define MPU9250_DEVICE_IRQ_TRIGGER    1

#if MPU9250_DEVICE_IRQ_TRIGGER
#define MPU9250_SMD_IRQ_TRIGGER    0
#else
#define MPU9250_SMD_IRQ_TRIGGER    1
#endif

#define MPU9250_FIFO_SINGLE_READ_MAX_BYTES 256

struct inv_mpu9250_platform_data {
	__s8 orientation[9];
};


/*register and associated bit definition*/


#define INV_MPU9250_BIT_ACCEL_OUT                   0x08
#define INV_MPU9250_BITS_GYRO_OUT                   0x70
#define INV_MPU9250_BITS_TEMP_OUT                   0x80
#define INV_MPU9250_BIT_DATA_RDY_EN                 0x01
#define INV_MPU9250_BIT_DMP_INT_EN                  0x02
#define INV_MPU9250_BIT_FIFO_RST                    0x04
#define INV_MPU9250_BIT_I2C_MST_DIS                  0x10
#define INV_MPU9250_BIT_FIFO_EN                     0x40
#define INV_MPU9250_BIT_H_RESET                     0x80
#define INV_MPU9250_BIT_SLEEP                       0x40
#define INV_MPU9250_BIT_CLK_MASK                    0x7
#define INV_MPU9250_BIT_PWR_ACCL_STBY               0x38
#define INV_MPU9250_BIT_PWR_GYRO_STBY               0x07
#define INV_MPU9250_BYTES_PER_3AXIS_SENSOR          6
#define INV_MPU9250_FIFO_COUNT_BYTE                 2
#define INV_MPU9250_FIFO_THRESHOLD                  500
#define INV_MPU9250_POWER_UP_TIME                   100
#define INV_MPU9250_TEMP_UP_TIME                    100
#define INV_MPU9250_SENSOR_UP_TIME                  20

#define INV_MPU9250_TEMP_OFFSET              12421
#define INV_MPU9250_TEMP_SCALE               2941
#define INV_MPU9250_MAX_GYRO_FS_PARAM        3
#define INV_MPU9250_MAX_ACCL_FS_PARAM        3
#define INV_MPU9250_THREE_AXIS               3
#define INV_MPU9250_GYRO_CONFIG_FSR_SHIFT    3
#define INV_MPU9250_ACCL_CONFIG_FSR_SHIFT    3

/* 6 + 6 round up and plus 8 */
#define INV_MPU9250_OUTPUT_DATA_SIZE         24
#define INV_MPU9250_MAX_FIFO_OUTPUT          256
#define INV_MPU9250_STORED_DATA              512

/* init parameters */
#define INV_MPU9250_INIT_FIFO_RATE           50  /* data rdy rate */
#define INV_MPU9250_MAX_FIFO_RATE            1000
#define INV_MPU9250_MIN_FIFO_RATE            4
#define INV_MPU9250_GYRO_8K_RATE             8000

#define INV_MPU9250_TIME_STAMP_TOR           5
#define INV_MPU9250_ONE_K_HZ                 1000

/* scan element definition */
enum inv_mpu9250_scan {
	INV_MPU9250_SCAN_ACCL_X,
	INV_MPU9250_SCAN_ACCL_Y,
	INV_MPU9250_SCAN_ACCL_Z,
	INV_MPU9250_SCAN_GYRO_X,
	INV_MPU9250_SCAN_GYRO_Y,
	INV_MPU9250_SCAN_GYRO_Z,
	INV_MPU9250_SCAN_TEMP,
	INV_MPU9250_SCAN_TIMESTAMP,
	INV_MPU9250_SCAN_MGAN_X,
	INV_MPU9250_SCAN_MGAN_Y,
	INV_MPU9250_SCAN_MGAN_Z,
};

/* IIO attribute address */
enum INV_MPU9250_IIO_ATTR_ADDR {
	ATTR_MPU9250_GYRO_MATRIX,
	ATTR_MPU9250_ACCL_MATRIX,
};

enum inv_mpu9250_clock_sel_e {
	INV_MPU9250_CLK_INTERNAL = 0,
	INV_MPU9250_CLK_PLL,
	INV_NUM_CLK
};

enum inv_mpu9250_spi_freq {
	MPU_SPI_FREQUENCY_1MHZ = 960000UL,
	MPU_SPI_FREQUENCY_5MHZ = 960000UL,
	MPU_SPI_FREQUENCY_8MHZ = 8000000UL,
	MPU_SPI_FREQUENCY_10MHZ = 10000000UL,
	MPU_SPI_FREQUENCY_15MHZ = 15000000UL,
	MPU_SPI_FREQUENCY_20MHZ = 20000000UL,
};

#define MPU_SPI_BUF_LEN   512
#define MPU9250_DEV_NAME  "mpu9250_iio"

struct mpu9250_chip_config {
	enum gyro_lpf_e            gyro_lpf;
	enum acc_lpf_e             acc_lpf;
	enum gyro_fsr_e            gyro_fsr;
	enum acc_fsr_e             acc_fsr;
	enum gyro_sample_rate_e    gyro_sample_rate;
	bool                       compass_enabled;
	enum compass_sample_rate_e compass_sample_rate;
	bool                       fifo_enabled;
	uint8_t                    fifo_en_mask;
};

/**
 *  struct inv_mpu9250_hw - Other important hardware information.
 *  @num_reg:   Number of registers on device.
 *  @name:      name of the chip.
 *  @reg:       register map of the chip.
 *  @config:    configuration of the chip.
 */
struct inv_mpu9250_hw {
	u8 num_reg;
	u8 *name;
	const struct inv_mpu9250_reg_map *reg;
};

struct mpu9250_data {
	uint64_t timestamp;
	int16_t  temperature_raw;
	float    temperature;
	int16_t  accel_raw[3];
	float    accel_scaling;
	float    accel_range_m_s2;
	int16_t  gyro_raw[3];
	float    gyro_range_rad_s;
	float    gyro_scaling;
	bool     mag_data_ready;
	int16_t  mag_raw[3];
	float    mag_range_ga;
	float    mag_scaling;
	uint8_t  compass_sens_adj[3];
};

/*
 *  struct inv_mpu9250_state - Driver state variables.
 *  @TIMESTAMP_FIFO_SIZE:  fifo size for timestamp.
 *  @trig:                 IIO trigger.
 *  @chip_config:          Cached attribute information.
 *  @reg:                  Map of important registers.
 *  @hw:                   Other hardware-specific information.
 *  @chip_type:            chip type.
 *  @time_stamp_lock:      spin lock to time stamp.
 *  @spi:                  spi devices
 *  @plat_data:            platform data.
 *  @timestamps:           kfifo queue to store time stamp.
 */
struct inv_mpu9250_state {
#define TIMESTAMP_FIFO_SIZE 32
	struct iio_trigger  *trig;
	const struct inv_mpu9250_reg_map *reg;
	const struct inv_mpu9250_hw *hw;
	struct mpu9250_chip_config *config;
	struct mpu9250_data raw_data;
	struct reg_cfg reg_cfg_info;
	spinlock_t time_stamp_lock;
	struct spi_device *spi;
	struct inv_mpu9250_platform_data plat_data;
	uint8_t *tx_buf;   /* used for spi transaction */
	uint8_t *rx_buf;   /* used for spi transaction */
	uint8_t fifo_packet_size;
	int fifo_cnt_threshold;
	enum   inv_devices chip_type;
	int gpio;
	DECLARE_KFIFO(timestamps, long long, TIMESTAMP_FIFO_SIZE);
};

extern struct iio_trigger *inv_trig;
irqreturn_t inv_mpu9250_irq_handler(int irq, void *p);
irqreturn_t inv_mpu9250_read_fifo_fn(int irq, void *p);
int inv_mpu9250_probe_trigger(struct iio_dev *indio_dev);
void inv_mpu9250_remove_trigger(struct inv_mpu9250_state *st);
int inv_mpu9250_reset_fifo(struct iio_dev *indio_dev);
int inv_mpu9250_switch_engine(struct inv_mpu9250_state *st,
				bool en, u32 mask);
int inv_mpu9250_write_reg(struct inv_mpu9250_state *st,
				int reg, u8 value);
int inv_mpu9250_read_reg(struct inv_mpu9250_state *st,
				uint8_t reg, uint8_t *val);
int mpu9250_bulk_read(struct inv_mpu9250_state *st,
				int reg, char *buf, int size);
int mpu9250_start_fifo(struct inv_mpu9250_state *st);
int inv_mpu9250_set_power_itg(struct inv_mpu9250_state *st, bool power_on);
int inv_mpu9250_get_interrupt_status(struct inv_mpu9250_state *st);
extern int inv_mpu9250_spi_bulk_read(struct inv_mpu9250_state *st,
		int reg, uint8_t length, uint8_t *buf);
#endif
