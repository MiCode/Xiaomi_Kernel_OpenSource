/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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
 *      @file  inv_gyro.h
 *      @brief Struct definitions for the Invensense gyro driver.
 */

#ifndef _INV_GYRO_H_
#define _INV_GYRO_H_

#include <linux/i2c.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/spinlock.h>
#include <linux/mpu.h>
#include <linux/regulator/consumer.h>
#include "dmpKey.h"


#define GYRO_INPUT_RESOLUTION		(1)
#define ACCL_INPUT_RESOLUTION		(1)
#define NVI_BYPASS_TIMEOUT_MS		(1000)
#define NVI_FIFO_SIZE_3050		(512)
#define NVI_FIFO_SIZE_6050		(1024)
#define NVI_FIFO_SIZE_6500		(4096)
#define NVI_FIFO_SAMPLE_SIZE_MAX	(38)
#define NVI_DELAY_US_MAX		(256000)
#define NVI_DELAY_US_MIN		(10000)
#define NVI_DELAY_DEFAULT		(100000)
#define NVI_INPUT_GYRO_DELAY_US_MIN	(5000)
#define NVI_INPUT_ACCL_DELAY_US_MIN	(5000)
#define NVI_TEMP_EN			(1 << 0)
#define NVI_TEMP_GYRO			(1 << 1)
#define NVI_TEMP_ACCL			(1 << 2)
#define NVI_MOT_DIS			(0)
#define NVI_MOT_EN			(1)
#define NVI_MOT_DBG			(2)

#define NVI_PM_ERR			(0)
#define NVI_PM_AUTO			(1)
#define NVI_PM_OFF_FORCE		(2)
#define NVI_PM_OFF			(3)
#define NVI_PM_STDBY			(4)
#define NVI_PM_ON_CYCLE			(5)
#define NVI_PM_ON			(6)
#define NVI_PM_ON_FULL			(7)

#define AXIS_X				(0)
#define AXIS_Y				(1)
#define AXIS_Z				(2)

#define NVI_DBG_SPEW_MSG		(1 << 0)
#define NVI_DBG_SPEW_AUX		(1 << 1)
#define NVI_DBG_SPEW_GYRO		(1 << 2)
#define NVI_DBG_SPEW_TEMP		(1 << 3)
#define NVI_DBG_SPEW_ACCL		(1 << 4)
#define NVI_DBG_SPEW_ACCL_RAW		(1 << 5)
#define NVI_DBG_SPEW_FIFO		(1 << 6)

enum NVI_DATA_INFO {
	NVI_DATA_INFO_DATA = 0,
	NVI_DATA_INFO_VER,
	NVI_DATA_INFO_RESET,
	NVI_DATA_INFO_REGS,
	NVI_DATA_INFO_DBG,
	NVI_DATA_INFO_AUX_SPEW,
	NVI_DATA_INFO_GYRO_SPEW,
	NVI_DATA_INFO_TEMP_SPEW,
	NVI_DATA_INFO_ACCL_SPEW,
	NVI_DATA_INFO_ACCL_RAW_SPEW,
	NVI_DATA_INFO_FIFO_SPEW,
	NVI_DATA_INFO_LIMIT_MAX,
};

/**
 *  struct inv_reg_map_s - Notable slave registers.
 *  @who_am_i:		Upper 6 bits of the device's slave address.
 *  @sample_rate_div:	Divider applied to gyro output rate.
 *  @lpf:		Configures internal LPF.
 *  @product_id:	Product revision.
 *  @bank_sel:		Selects between memory banks.
 *  @user_ctrl:		Enables/resets the FIFO.
 *  @fifo_en:		Determines which data will appear in FIFO.
 *  @gyro_config:	gyro config register.
 *  @accl_config:	accel config register
 *  @fifo_count_h:	Upper byte of FIFO count.
 *  @fifo_r_w:		FIFO register.
 *  @raw_gyro		Address of first gyro register.
 *  @raw_accl		Address of first accel register.
 *  @temperature	temperature register
 *  @int_enable:	Interrupt enable register.
 *  @int_status:	Interrupt flags.
 *  @pwr_mgmt_1:	Controls chip's power state and clock source.
 *  @pwr_mgmt_2:	Controls power state of individual sensors.
 *  @mem_start_addr:	Address of first memory read.
 *  @mem_r_w:		Access to memory.
 *  @prgm_strt_addrh	firmware program start address register
 */
struct inv_reg_map_s {
	u8 who_am_i;
	u8 sample_rate_div;
	u8 lpf;
	u8 product_id;
	u8 bank_sel;
	u8 user_ctrl;
	u8 fifo_en;
	u8 gyro_config;
	u8 accl_config;
	u8 fifo_count_h;
	u8 fifo_r_w;
	u8 raw_gyro;
	u8 raw_accl;
	u8 temperature;
	u8 int_enable;
	u8 int_status;
	u8 pwr_mgmt_1;
	u8 pwr_mgmt_2;
	u8 mem_start_addr;
	u8 mem_r_w;
	u8 prgm_strt_addrh;

	u8 accl_fifo_en;
	u8 fifo_reset;
	u8 i2c_mst_reset;
	u8 cycle;
	u8 temp_dis;
};

enum inv_devices {
	INV_ITG3500,
	INV_MPU3050,
	INV_MPU6050,
	INV_MPU9150,
	INV_MPU6500,
	INV_MPU9250,
	INV_MPU9350,
	INV_MPU6515,
	INV_MPU6XXX,
	INV_NUM_PARTS
};

struct nvi_hal {
	unsigned int fifo_size;
	unsigned long *lpa_tbl;
	unsigned int lpa_tbl_n;
};

/**
 *  struct test_setup_t - set up parameters for self test.
 *  @sample_rate: sensitity for gyro.
 *  @sample_rate: sample rate, i.e, fifo rate.
 *  @lpf:	low pass filter.
 *  @gyro_fsr:	full scale range.
 *  @accl_fsr:	accel full scale range.
 *  @accl_sens:	accel sensitivity
 */
struct test_setup_t {
	int gyro_sens;
	int sample_rate;
	int lpf;
	int gyro_fsr;
	int accl_fsr;
	unsigned int accl_sens[3];
};

/**
 *  struct inv_hw_s - Other important hardware information.
 *  @num_reg:	Number of registers on device.
 *  @name:      name of the chip
 */
struct inv_hw_s {
	unsigned char num_reg;
	unsigned char *name;
};

/**
 *  struct inv_chip_config_s - Cached chip configuration data.
 *  @gyro_fsr:		Full scale range.
 *  @lpf:		Digital low pass filter frequency.
 *  @clk_src:		Clock source.
 *  @accl_fsr:		accel full scale range.
 *  @fifo_rate:		FIFO update rate.
 *  @enable:		master enable to enable output
 *  @accl_enable:	enable accel functionality
 *  @accl_fifo_enable:	enable accel data output
 *  @gyro_enable:	enable gyro functionality
 *  @gyro_fifo_enable:	enable gyro data output
 *  @compass_enable:	enable compass
 *  @is_asleep:		1 if chip is powered down.
 *  @dmp_on:		dmp is on/off
 *  @firmware_loaded:	flag indicate firmware loaded or not.
 *  @lpa_mod:		low power mode
 *  @lpa_freq:          low power accel frequency.
 *  @prog_start_addr:	firmware program start address
 */
struct inv_chip_config_s {
	unsigned char lpf;
	unsigned char clk_src;
	unsigned char enable;
	unsigned char gyro_enable;
	unsigned char gyro_fifo_enable;
	unsigned long gyro_delay_us;
	unsigned int gyro_resolution;
	unsigned char gyro_fsr;
	unsigned char accl_enable;
	unsigned char accl_fifo_enable;
	unsigned long accl_delay_us;
	unsigned int accl_resolution;
	unsigned char accl_fsr;
	unsigned long lpa_delay_us;
	unsigned char temp_enable;
	unsigned char temp_fifo_enable;
	unsigned char dmp_on;
	unsigned char firmware_loaded;
	unsigned char mot_enable;
	unsigned char mot_dur;
	unsigned char mot_ctrl;
	unsigned int mot_cnt;
	unsigned int fifo_thr;
	unsigned int  prog_start_addr;
	unsigned long min_delay_us;
	s64 gyro_start_delay_ns;
	unsigned int bypass_timeout_ms;
	unsigned char is_asleep;
};

/**
 *  struct inv_chip_info_s - Chip related information.
 *  @product_id:	Product id.
 *  @product_revision:	Product revision.
 *  @silicon_revision:	Silicon revision.
 *  @software_revision:	software revision.
 *  @multi:		accel specific multiplier.
 *  @compass_sens:	compass sensitivity.
 *  @gyro_sens_trim:	Gyro sensitivity trim factor.
 *  @accl_sens_trim:    accel sensitivity trim factor.
 */
struct inv_chip_info_s {
	unsigned char product_id;
	unsigned char product_revision;
	unsigned char silicon_revision;
	unsigned char software_revision;
	unsigned char multi;
	unsigned char compass_sens[3];
	unsigned long gyro_sens_trim;
	unsigned long accl_sens_trim;
};

/**
 *  struct inv_trigger_s - Variables passed between interrupt and kernel space.
 *  @irq:		Interrupt number.
 *  @timestamps:	Timestamp buffer.
 */
struct inv_trigger_s {
#define TIMESTAMP_FIFO_SIZE 32
	unsigned long irq;
	DECLARE_KFIFO(timestamps, long long, TIMESTAMP_FIFO_SIZE);
};

/**
 *  struct inv_flick_s structure to store flick data.
 *  @lower:	lower bound of flick.
 *  @upper:     upper bound of flick.
 *  @counter:	counterof flick.
 *  @int_on:	interrupt on for flick.
 *  @msg_on;    message to carry flick
 *  @axis:      axis of flick
 */
struct inv_flick_s {
	int lower;
	int upper;
	int counter;
	char int_on;
	char msg_on;
	char axis;
};

/**
 *  struct inv_tap_s structure to store tap data.
 *  @tap_on:	tap on
 *  @min_taps:  minimut taps counted.
 *  @thresh:    tap threshold.
 *  @time:	tap time.
 */
struct inv_tap_s {
	char tap_on;
	char min_tap;
	short thresh;
	short time;
};

/**
 * struct inv_regulator_s structure to store regulator
 */
struct inv_regulator_s {
	struct regulator *regulator_vlogic;
	struct regulator *regulator_vdd;
};

#define AUX_PORT_MAX			(5)
#define AUX_PORT_SPECIAL		(4)
#define AUX_PORT_BYPASS			(-1)
#define AUX_EXT_DATA_REG_MAX		(24)
#define AUX_DEV_VALID_READ_LOOP_MAX	(20)
#define AUX_DEV_VALID_READ_DELAY_MS	(5)

struct aux_port {
	struct nvi_mpu_port nmp;
	unsigned short ext_data_offset;
	bool hw_valid;
	bool hw_en;
	bool hw_do;
	bool enable;
	bool fifo_en;
};

struct aux_ports {
	struct aux_port port[AUX_PORT_MAX];
	s64 bypass_timeout_ns;
	unsigned int bypass_lock;
	u8 delay_hw;
	unsigned short ext_data_n;
	unsigned char ext_data[AUX_EXT_DATA_REG_MAX];
	unsigned char clock_i2c;
	bool reset_i2c;
	bool reset_fifo;
	bool enable;
	bool en3050;
};

struct nvi_hw {
	u8 aux_vddio;
	u8 smplrt_div;
	u8 config;
	u8 gyro_config;
	u8 accl_config;
	u8 accl_config2;
	u8 lposc_clksel;
	u8 mot_thr;
	u8 mot_dur;
	u8 zrmot_thr;
	u8 zrmot_dur;
	u8 fifo_en;
	u8 i2c_mst_ctrl;
	u8 i2c_slv_addr[AUX_PORT_MAX];
	u8 i2c_slv_reg[AUX_PORT_MAX];
	u8 i2c_slv_ctrl[AUX_PORT_SPECIAL];
	u8 i2c_slv4_do;
	u8 i2c_slv4_ctrl;
	u8 int_pin_cfg;
	u8 int_enable;
	u8 i2c_slv_do[AUX_PORT_SPECIAL];
	u8 i2c_mst_delay_ctrl;
	u8 mot_detect_ctrl;
	u8 user_ctrl;
	u8 pwr_mgmt_1;
	u8 pwr_mgmt_2;
};

struct inv_mpu_slave;
/**
 *  struct inv_gyro_state_s - Driver state variables.
 *  @chip_config:	Cached attribute information.
 *  @chip_info:		Chip information from read-only registers.
 *  @flick:		flick data structure
 *  @reg:		Map of important registers.
 *  @hw:		Other hardware-specific information.
 *  @idev:		Handle to input device.
 *  @idev_dmp:		Handle to input device for DMP.
 *  @idev_compass:	Handle to input device for compass.
 *  @chip_type:		chip type.
 *  @time_stamp_lock:	spin lock to time stamp.
 *  @inv_class:		store class handle.
 *  @inv_dev:		store device handle.
 *  @i2c:		i2c client handle.
 *  @plat_data:		platform data.
 *  @mpu_slave:		mpu slave handle.
 *  @fifo_counter:	MPU3050 specific work around.
 *  @has_compass:	has compass or not.
 *  @compass_scale:	compass scale.
 *  @i2c_addr:		i2c address.
 *  @compass_divider:	slow down compass rate.
 *  @compass_counter:	slow down compass rate.
 *  @sample_divider:    sample divider for dmp.
 *  @fifo_divider:      fifo divider for dmp.
 *  @sl_handle:		Handle to I2C port.
 *  @irq_dur_us:	duration between each irq.
 *  @last_isr_time:	last isr time.
 *  @early_suspend:     struct for early suspend.
 *  @early_suspend_enable: sysfs interface to store current early_suspend.
 *  @inv_regulator_s:	Regulator sturcture to store regulator.
 */
struct inv_gyro_state_s {
	struct inv_chip_config_s chip_config;
	struct inv_chip_info_s chip_info;
	struct inv_flick_s flick;
	struct inv_tap_s tap;
	struct inv_reg_map_s *reg;
	struct inv_hw_s *hw_s;
	struct inv_trigger_s trigger;
	struct input_dev *idev;
	struct input_dev *idev_dmp;
	enum   inv_devices chip_type;
	spinlock_t time_stamp_lock;
	struct class *inv_class;
	struct device *inv_dev;
	struct i2c_client *i2c;
	struct mpu_platform_data plat_data;
	struct inv_mpu_slave *mpu_slave;
	struct regulator_bulk_data vreg[2];
	struct notifier_block nb_vreg[2];
	s64 vreg_en_ts[2];
	unsigned char i2c_addr;
	unsigned char sample_divider;
	unsigned char fifo_divider;
	void *sl_handle;
	struct mutex mutex;
	struct mutex mutex_temp;
	struct nvi_hw hw;
	struct nvi_hal hal;
	struct aux_ports aux;
	int pm;
	unsigned long sample_delay_us;
	u16 fifo_sample_size;
	bool shutdown;
	bool suspend;
	bool stop_workqueue;
	bool fifo_reset_3050;
	bool mot_det_en;
	s64 fifo_ts;
	s64 gyro_start_ts;
	unsigned int data_info;
	unsigned int dbg;
#if DEBUG_SYSFS_INTERFACE
	u16 dbg_i2c_addr;
	u8 dbg_reg;
#endif /* DEBUG_SYSFS_INTERFACE */
	s64 temp_ts;
	s64 last_1000;
	s16 temp_val;
	s16 gyro[3];
	s16 accl[3];
	s16 accl_raw[3];
	u8 buf[NVI_FIFO_SAMPLE_SIZE_MAX * 2]; /* (* 2)=FIFO OVERFLOW OFFSET */
	bool irq_disabled;
	unsigned int num_int;
	struct work_struct work_struct;
};

/* produces an unique identifier for each device based on the
   combination of product version and product revision */
struct prod_rev_map_t {
	unsigned short mpl_product_key;
	unsigned char silicon_rev;
	unsigned short gyro_trim;
	unsigned short accel_trim;
};
/**
 *  struct inv_mpu_slave - MPU slave structure.
 *  @suspend:		suspend operation.
 *  @resume:		resume operation.
 *  @setup:		setup chip. initialization.
 *  @create_sysfs:	create chip specific sysfs entries.
 *  @remove_sysfs:	remove chip specific sysfs entries.
 *  @combine_data:	combine raw data into meaningful data.
 *  @get_mode:		get current chip mode.
 */
struct inv_mpu_slave {
	int (*suspend)(struct inv_gyro_state_s *);
	int (*resume)(struct inv_gyro_state_s *);
	int (*setup)(struct inv_gyro_state_s *);
	int (*combine_data)(unsigned char *in, short *out);
	int (*get_mode)(struct inv_gyro_state_s *);
	int (*set_lpf)(struct inv_gyro_state_s *, int rate);
	int (*set_fs)(struct inv_gyro_state_s *, int fs);
};

/* register definition*/
#define REG_3050_AUX_VDDIO      (0x13)
#define REG_3050_SLAVE_ADDR     (0x14)
#define REG_3050_SLAVE_REG      (0x18)
#define REG_3050_AUX_XOUT_H     (0x23)

#define REG_3500_OTP            (0x00)
#define REG_AUX_VDDIO           (0x01)
#define REG_ST_GCT_X            (0x0D)
#define REG_6500_LP_ACCEL_ODR   (0x1E)
#define REG_MOT_THR             (0x1F)
#define REG_MOT_DUR             (0x20)
#define REG_ZMOT_THR            (0x21)
#define REG_ZMOT_DUR            (0x22)
#define REG_I2C_MST_CTRL        (0x24)
#define BIT_SLV3_FIFO_EN        (0x20)
#define REG_I2C_SLV0_ADDR       (0x25)
#define REG_I2C_SLV0_REG        (0x26)
#define REG_I2C_SLV0_CTRL       (0x27)
#define BITS_I2C_SLV_CTRL_LEN   (0x0F)
#define BITS_I2C_SLV_REG_DIS    (0x10)
#define REG_I2C_SLV4_ADDR       (0x31)
#define REG_I2C_SLV4_REG        (0x32)
#define REG_I2C_SLV4_DO         (0x33)
#define REG_I2C_SLV4_CTRL       (0x34)
#define REG_I2C_SLV4_DI         (0x35)
#define REG_I2C_MST_STATUS      (0x36)
#define REG_I2C_SLV0_DO         (0x63)
#define REG_FIFO_EN             (0x23)

#define BITS_I2C_MST_DLY        (0x1F)
#define REG_INT_PIN_CFG         (0x37)
#define REG_DMP_INT_STATUS      (0x39)
#define REG_EXT_SENS_DATA_00    (0x49)
#define REG_I2C_MST_DELAY_CTRL  (0x67)
#define REG_MOT_DETECT_CTRL     (0x69)
#define REG_BANK_SEL            (0x6D)
#define REG_MEM_START           (0x6E)
#define REG_MEM_RW              (0x6F)

/* bit definitions */
#define BIT_3050_VDDIO          (0x04)
#define BIT_3050_AUX_IF_EN      (0x20)
#define BIT_3050_AUX_IF_RST     (0x08)
#define BIT_3050_FIFO_RST       (0x02)

#define BIT_BYPASS_EN           (0x2)
#define BIT_WAIT_FOR_ES         (0x40)
#define BIT_I2C_MST_P_NSR       (0x10)
#define BIT_I2C_READ            (0x80)
#define BIT_SLV_EN              (0x80)

#define BIT_DMP_EN              (0x80)
#define BIT_FIFO_EN             (0x40)
#define BIT_I2C_MST_EN          (0x20)
#define BIT_DMP_RST             (0x08)
#define BIT_FIFO_RST            (0x04)
#define BIT_I2C_MST_RST         (0x02)

#define BIT_SLV0_DLY_EN         (0x01)
#define BIT_SLV1_DLY_EN         (0x02)
#define BIT_DELAY_ES_SHADOW     (0x80)

#define BIT_MOT_EN              (0x40)
#define BIT_ZMOT_EN             (0x20)
#define BIT_FIFO_OVERFLOW       (0x10)
#define BIT_DATA_RDY_EN	        (0x01)
#define BIT_DMP_INT_EN          (0x02)

#define BIT_PWR_ACCL_STBY       (0x38)
#define BIT_PWR_GYRO_STBY       (0x07)

#define BIT_TEMP_FIFO_EN	(0x80)
#define BIT_GYRO_XOUT           (0x40)
#define BIT_GYRO_YOUT		(0x20)
#define BIT_GYRO_ZOUT		(0x10)
#define BIT_ACCEL_OUT		(0x08)
#define BITS_GYRO_OUT		(0x70)
#define BITS_SELF_TEST_EN       (0xE0)
#define BITS_3050_ACCL_OUT	(0x0E)
#define BIT_3050_FIFO_FOOTER	(0x01)
#define BITS_3050_POWER1        (0x30)
#define BITS_3050_POWER2        (0x10)
#define BITS_3050_GYRO_STANDBY  (0x38)
#define BITS_FSR		(0x18)
#define BITS_LPF		(0x07)
#define BITS_CLK		(0x07)
#define BIT_RESET               (0x80)
#define BIT_H_RESET		(0x80)
#define BIT_SLEEP		(0x40)
#define BIT_CYCLE               (0x20)
#define BIT_TEMP_DIS		(0x08)
#define BIT_LPA_FREQ            (0xC0)
#define BIT_STBY_XA		(0x20)
#define BIT_STBY_YA		(0x10)
#define BIT_STBY_ZA		(0x08)
#define BIT_STBY_XG		(0x04)
#define BIT_STBY_YG		(0x02)
#define BIT_STBY_ZG		(0x01)

#define DMP_START_ADDR          (0x400)
#define BYTES_FOR_DMP           (16)
#define BYTES_PER_SENSOR        (6)
#define FIFO_THRESHOLD           500
#define POWER_UP_TIME           (40)
#define REG_UP_TIME		(5)
#define POR_MS			(100)
#define MPU_MEM_BANK_SIZE        (256)
#define MPL_PROD_KEY(ver, rev) (ver * 100 + rev)
#define NUM_OF_PROD_REVS (ARRAY_SIZE(prod_rev_map))
/*---- MPU6050 Silicon Revisions ----*/
#define MPU_SILICON_REV_A2              1       /* MPU6050A2 Device */
#define MPU_SILICON_REV_B1              2       /* MPU6050B1 Device */

#define MPU6050_ID			(0x68)
#define MPU6500_ID			(0x70)
#define MPU6500_PRODUCT_REVISION	(1)
#define MPU6500_MEM_REV_ADDR		(0x17)
#define MPU9250_ID			(0x71)
#define MPU6515_ID			(0x74)

#define BIT_PRFTCH_EN                           0x40
#define BIT_CFG_USER_BANK                       0x20
#define BITS_MEM_SEL                            0x1f
/* time stamp tolerance */
#define TIME_STAMP_TOR           (5)
#define MAX_CATCH_UP             (5)
#define DEFAULT_ACCL_TRIM        (16384)
#define DEFAULT_GYRO_TRIM        (131)
#define MAX_FIFO_RATE            (1000000)
#define MIN_FIFO_RATE            (4000)
#define ONE_K_HZ                 (1000)

/* authenticate key */
#define D_AUTH_OUT               (32)
#define D_AUTH_IN                (36)
#define D_AUTH_A                 (40)
#define D_AUTH_B                 (44)

/* flick related defines */
#define DATA_INT            (2097)
#define DATA_MSG_ON         (262144)

/*tap related defines */
#define INV_TAP                               0x08
#define INV_NUM_TAP_AXES (3)

#define INV_TAP_AXIS_X_POS                    0x20
#define INV_TAP_AXIS_X_NEG                    0x10
#define INV_TAP_AXIS_Y_POS                    0x08
#define INV_TAP_AXIS_Y_NEG                    0x04
#define INV_TAP_AXIS_Z_POS                    0x02
#define INV_TAP_AXIS_Z_NEG                    0x01
#define INV_TAP_ALL_DIRECTIONS                0x3f

#define INV_TAP_AXIS_X                        0x1
#define INV_TAP_AXIS_Y                        0x2
#define INV_TAP_AXIS_Z                        0x4

#define INV_TAP_AXIS_ALL                      \
		(INV_TAP_AXIS_X            |   \
		INV_TAP_AXIS_Y             |   \
		INV_TAP_AXIS_Z)

#define INT_SRC_TAP    0x01
#define INT_SRC_ORIENT 0x02

/*orientation related */
#define INV_X_UP                          0x01
#define INV_X_DOWN                        0x02
#define INV_Y_UP                          0x04
#define INV_Y_DOWN                        0x08
#define INV_Z_UP                          0x10
#define INV_Z_DOWN                        0x20
#define INV_ORIENTATION_ALL               0x3F

#define INV_ORIENTATION_FLIP              0x40
#define INV_X_AXIS_INDEX                 (0x00)
#define INV_Y_AXIS_INDEX                 (0x01)
#define INV_Z_AXIS_INDEX                 (0x02)

#define INV_ELEMENT_1                    (0x0001)
#define INV_ELEMENT_2                    (0x0002)
#define INV_ELEMENT_3                    (0x0004)
#define INV_ELEMENT_4                    (0x0008)
#define INV_ELEMENT_5                    (0x0010)
#define INV_ELEMENT_6                    (0x0020)
#define INV_ELEMENT_7                    (0x0040)
#define INV_ELEMENT_8                    (0x0080)
#define INV_ALL                          (0xFFFF)
#define INV_ELEMENT_MASK                 (0x00FF)
#define INV_GYRO_ACC_MASK                (0x007E)

enum inv_filter_e {
	INV_FILTER_256HZ_NOLPF2 = 0,
	INV_FILTER_188HZ,
	INV_FILTER_98HZ,
	INV_FILTER_42HZ,
	INV_FILTER_20HZ,
	INV_FILTER_10HZ,
	INV_FILTER_5HZ,
	INV_FILTER_2100HZ_NOLPF,
	NUM_FILTER
};
/*==== MPU6050B1 MEMORY ====*/
enum MPU_MEMORY_BANKS {
	MEM_RAM_BANK_0 = 0,
	MEM_RAM_BANK_1,
	MEM_RAM_BANK_2,
	MEM_RAM_BANK_3,
	MEM_RAM_BANK_4,
	MEM_RAM_BANK_5,
	MEM_RAM_BANK_6,
	MEM_RAM_BANK_7,
	MEM_RAM_BANK_8,
	MEM_RAM_BANK_9,
	MEM_RAM_BANK_10,
	MEM_RAM_BANK_11,
	MPU_MEM_NUM_RAM_BANKS,
	MPU_MEM_OTP_BANK_0 = 16
};

enum inv_fsr_e {
	INV_FSR_250DPS = 0,
	INV_FSR_500DPS,
	INV_FSR_1000DPS,
	INV_FSR_2000DPS,
	NUM_FSR
};
enum inv_accl_fs_e {
	INV_FS_02G = 0,
	INV_FS_04G,
	INV_FS_08G,
	INV_FS_16G,
	NUM_ACCL_FSR
};

enum inv_clock_sel_e {
	INV_CLK_INTERNAL = 0,
	INV_CLK_PLL,
	NUM_CLK
};

int inv_hw_self_test(struct inv_gyro_state_s *st, int *gyro_bias_regular);
int inv_get_silicon_rev_mpu6050(struct inv_gyro_state_s *st);
int inv_get_silicon_rev_mpu6500(struct inv_gyro_state_s *st);
int inv_i2c_read_base(struct inv_gyro_state_s *st, unsigned short i2c_addr,
	unsigned char reg, unsigned short length, unsigned char *data);
int inv_i2c_single_write_base(struct inv_gyro_state_s *st,
	unsigned short i2c_addr, unsigned char reg, unsigned char data);
#define inv_i2c_read(st, reg, len, data) \
	inv_i2c_read_base(st, st->i2c_addr, reg, len, data)
#define inv_i2c_single_write(st, reg, data) \
	inv_i2c_single_write_base(st, st->i2c_addr, reg, data)
#define inv_secondary_read(reg, len, data) \
	inv_i2c_read_base(st, st->plat_data.secondary_i2c_addr, reg, len, data)
#define inv_secondary_write(reg, data) \
	inv_i2c_single_write_base(st, st->plat_data.secondary_i2c_addr, \
		reg, data)
s64 nvi_ts_ns(void);
int nvi_smplrt_div_wr(struct inv_gyro_state_s *inf, u8 smplrt_div);
int nvi_config_wr(struct inv_gyro_state_s *inf, u8 val);
int nvi_gyro_config_wr(struct inv_gyro_state_s *inf, u8 test, u8 fsr);
int nvi_accel_config_wr(struct inv_gyro_state_s *inf, u8 test, u8 fsr, u8 hpf);
int nvi_fifo_en_wr(struct inv_gyro_state_s *inf, u8 fifo_en);
int nvi_int_enable_wr(struct inv_gyro_state_s *inf, bool enable);
int nvi_user_ctrl_reset_wr(struct inv_gyro_state_s *inf, u8 val);
int nvi_user_ctrl_en_wr(struct inv_gyro_state_s *inf, u8 val);
int nvi_user_ctrl_en(struct inv_gyro_state_s *inf,
		     bool fifo_enable, bool i2c_enable);
int nvi_pm_wr(struct inv_gyro_state_s *inf,
	      u8 pwr_mgmt_1, u8 pwr_mgmt_2, u8 lpa);
int nvi_gyro_enable(struct inv_gyro_state_s *inf,
		    unsigned char enable, unsigned char fifo_enable);
int nvi_accl_enable(struct inv_gyro_state_s *inf,
		    unsigned char enable, unsigned char fifo_enable);
void nvi_report_temp(struct inv_gyro_state_s *inf, u8 *data, s64 ts);

int mpu_memory_write(struct i2c_adapter *i2c_adap,
			    unsigned char mpu_addr,
			    unsigned short mem_addr,
			    unsigned int len, unsigned char const *data);
int mpu_memory_read(struct i2c_adapter *i2c_adap,
			   unsigned char mpu_addr,
			   unsigned short mem_addr,
			   unsigned int len, unsigned char *data);
void inv_setup_reg_mpu3050(struct inv_reg_map_s *reg);
int inv_init_config_mpu3050(struct inv_gyro_state_s *st);
int set_3050_bypass(struct inv_gyro_state_s *st, int enable);
int inv_register_kxtf9_slave(struct inv_gyro_state_s *st);
s64 get_time_ns(void);
int inv_get_accl_bias(struct inv_gyro_state_s *st, int *accl_bias_regular);

int inv_enable_tap_dmp(struct inv_gyro_state_s *st, unsigned char on);
int inv_enable_orientation_dmp(struct inv_gyro_state_s *st);
unsigned short inv_dmp_get_address(unsigned short key);

ssize_t inv_dmp_firmware_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size);

ssize_t inv_dmp_firmware_read(struct file *filp, struct kobject *kobj,
			      struct bin_attribute *bin_attr, char *buf,
			      loff_t off, size_t count);

#define mem_w(a, b, c) mpu_memory_write(st->sl_handle, \
			st->i2c_addr, a, b, c)
#define mem_w_key(key, b, c) mpu_memory_write(st->sl_handle, \
			st->i2c_addr, inv_dmp_get_address(key), b, c)

#endif  /* #ifndef _INV_GYRO_H_ */

