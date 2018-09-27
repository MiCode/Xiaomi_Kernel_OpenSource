/*
 * Copyright (C) 2012-2018 InvenSense, Inc.
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

#ifndef _INV_MPU_IIO_H_
#define _INV_MPU_IIO_H_

#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0))
#define KERNEL_VERSION_4_X
#endif

#include <linux/i2c.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/iio/imu/mpu.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#else
#include <linux/pm_wakeup.h>
#endif
#include <linux/wait.h>

#include <linux/iio/sysfs.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/input.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#ifdef CONFIG_INV_MPU_IIO_ICM20648
#include "icm20648/dmp3Default.h"
#endif
#ifdef CONFIG_INV_MPU_IIO_ICM20608D
#include "icm20608d/dmp3Default_20608D.h"
#endif

#include "inv_test/inv_counters.h"

#if defined(CONFIG_INV_MPU_IIO_ICM20648)
#include "icm20648/inv_mpu_iio_reg_20648.h"
#elif defined(CONFIG_INV_MPU_IIO_ICM20602)
#include "icm20602/inv_mpu_iio_reg_20602.h"
#elif defined(CONFIG_INV_MPU_IIO_ICM20608D)
#include "icm20608d/inv_mpu_iio_reg_20608.h"
#elif defined(CONFIG_INV_MPU_IIO_ICM20690)
#include "icm20690/inv_mpu_iio_reg_20690.h"
#elif defined(CONFIG_INV_MPU_IIO_IAM20680)
#include "iam20680/inv_mpu_iio_reg_20680.h"
#endif

#define INVENSENSE_DRIVER_VERSION		"8.1.2-simple-test1"

/* #define DEBUG */

/* #define ACCEL_BIAS_TEST */

/* #define BIAS_CONFIDENCE_HIGH 1 */

#define MAX_FIFO_READ_SIZE       128
#define MAX_DMP_READ_SIZE        16

/* data header defines */
#define WAKE_HDR                 0x8000

#define ACCEL_HDR                1
#define GYRO_HDR                 2
#define COMPASS_HDR              3
#define ALS_HDR                  4
#define SIXQUAT_HDR              5
#define NINEQUAT_HDR             6
#define PEDQUAT_HDR              7
#define GEOMAG_HDR               8
#define PRESSURE_HDR             9
#define GYRO_CALIB_HDR           10
#define COMPASS_CALIB_HDR        11
#define STEP_COUNTER_HDR         12
#define STEP_DETECTOR_HDR        13
#define STEP_COUNT_HDR           14
#define ACTIVITY_HDR             15
#define PICK_UP_HDR              16
#define EMPTY_MARKER             17
#define END_MARKER               18
#define COMPASS_ACCURACY_HDR     19
#define ACCEL_ACCURACY_HDR       20
#define GYRO_ACCURACY_HDR        21
#define EIS_GYRO_HDR             36
#define EIS_CALIB_HDR            37
#define LPQ_HDR                  38

#define ACCEL_WAKE_HDR           (ACCEL_HDR | WAKE_HDR)
#define GYRO_WAKE_HDR            (GYRO_HDR | WAKE_HDR)
#define COMPASS_WAKE_HDR         (COMPASS_HDR | WAKE_HDR)
#define ALS_WAKE_HDR             (ALS_HDR | WAKE_HDR)
#define SIXQUAT_WAKE_HDR         (SIXQUAT_HDR | WAKE_HDR)
#define NINEQUAT_WAKE_HDR        (NINEQUAT_HDR | WAKE_HDR)
#define PEDQUAT_WAKE_HDR         (PEDQUAT_HDR | WAKE_HDR)
#define GEOMAG_WAKE_HDR          (GEOMAG_HDR | WAKE_HDR)
#define PRESSURE_WAKE_HDR        (PRESSURE_HDR | WAKE_HDR)
#define GYRO_CALIB_WAKE_HDR      (GYRO_CALIB_HDR | WAKE_HDR)
#define COMPASS_CALIB_WAKE_HDR   (COMPASS_CALIB_HDR | WAKE_HDR)
#define STEP_COUNTER_WAKE_HDR    (STEP_COUNTER_HDR | WAKE_HDR)
#define STEP_DETECTOR_WAKE_HDR   (STEP_DETECTOR_HDR | WAKE_HDR)

/* init parameters */
#define MPU_INIT_SMD_THLD        1500
#define MPU_INIT_GYRO_SCALE      3
#define MPU_INIT_ACCEL_SCALE     2
#define MPU_INIT_PED_INT_THRESH  2
#define MPU_INIT_PED_STEP_THRESH 6
#define MPU_4X_TS_GYRO_SHIFT      (3160000 / 2)
#define DMP_START_ADDR_20645     0x900
#define DMP_START_ADDR_20648     0x1000
#define DMP_START_ADDR_10340     0x0a60
#define DMP_START_ADDR_20608D    0x4B0
#define MAX_WR_SZ                  100
#define WOM_DELAY_THRESHOLD      200
#define INV_ODR_BUFFER_MULTI     20
#define INV_ODR_OVER_FACTOR      20

#define COVARIANCE_SIZE          14
#define ACCEL_COVARIANCE_SIZE  (COVARIANCE_SIZE * sizeof(int))

#ifdef CONFIG_ENABLE_IAM_ACC_GYRO_BUFFERING
#define INV_ACC_MAXSAMPLE        4000
#define INV_GYRO_MAXSAMPLE       4000
#define G_MAX                    23920640
struct inv_acc_sample {
	int xyz[3];
	unsigned int tsec;
	unsigned long long tnsec;
};
struct inv_gyro_sample {
	int xyz[3];
	unsigned int tsec;
	unsigned long long tnsec;
};
enum {
	ACCEL_FSR_2G = 0,
	ACCEL_FSR_4G = 1,
	ACCEL_FSR_8G = 2,
	ACCEL_FSR_16G = 3
};
enum {
	GYRO_FSR_250DPS = 0,
	GYRO_FSR_500DPS = 1,
	GYRO_FSR_1000DPS = 2,
	GYRO_FSR_2000DPS = 3
};
#endif

enum inv_bus_type {
	BUS_IIO_I2C = 0,
	BUS_IIO_SPI,
};

struct inv_mpu_state;

enum INV_ENGINE {
	ENGINE_GYRO = 0,
	ENGINE_ACCEL,
	ENGINE_PRESSURE,
	ENGINE_I2C,
	ENGINE_NUM_MAX,
};

/**
 *  struct inv_hw_s - Other important hardware information.
 *  @num_reg:	Number of registers on device.
 *  @name:      name of the chip
 */
struct inv_hw_s {
	u8 num_reg;
	u8 *name;
};

/**
 *  struct inv_sensor - information for each sensor.
 *  @ts: this sensors timestamp.
 *  @ts_adj: sensor timestamp adjustment.
 *  @previous_ts: previous timestamp for this sensor.
 *  @dur: duration between samples in ns.
 *  @rate:  sensor data rate.
 *  @sample_size: number of bytes for the sensor.
 *  @odr_addr: output data rate address in DMP.
 *  @counter_addr: output counter address in DMP.
 *  @output: output on/off control word.
 *  @time_calib: calibrate timestamp.
 *  @sample_calib: calibrate bytes accumulated.
 *  @div:         divider in DMP mode.
 *  @calib_flag:  calibrate flag used to improve the accuracy of estimation.
 *  @on:    sensor on/off.
 *  @a_en:  accel engine requirement.
 *  @g_en:  gyro engine requirement.
 *  @c_en:  compass_engine requirement.
 *  @p_en:  pressure engine requirement.
 *  @engine_base: engine base for this sensor.
 *  @count: number of samples in one session.
 *  @send: decide whether to send this sample or not.
 */
struct inv_sensor {
	u64 ts;
	s64 ts_adj;
	u64 previous_ts;
	int dur;
	int rate;
	u8 sample_size;
	int odr_addr;
	int counter_addr;
	u16 output;
	u64 time_calib;
	u32 sample_calib;
	int div;
	bool calib_flag;
	bool on;
	bool a_en;
	bool g_en;
	bool c_en;
	bool p_en;
	enum INV_ENGINE engine_base;
	int count;
	bool send;
};

/**
 *  struct inv_sensor - information for each sensor.
 *  @sample_size: number of bytes for the sensor.
 *  @output: output on/off control word.
 *  @on:    sensor on/off.
 *  @header: accuracy header for communicate with HAL
 *dd  @count: number of samples in one session.
 */
struct inv_sensor_accuracy {
	u16 output;
	u8 sample_size;
	bool on;
	u16 header;
};

enum SENSOR_ACCURACY {
	SENSOR_ACCEL_ACCURACY = 0,
	SENSOR_GYRO_ACCURACY,
	SENSOR_COMPASS_ACCURACY,
	SENSOR_ACCURACY_NUM_MAX,
};

enum SENSOR_L {
	SENSOR_L_ACCEL = 0,
	SENSOR_L_GYRO,
	SENSOR_L_MAG,
	SENSOR_L_ALS,
	SENSOR_L_SIXQ,
	SENSOR_L_THREEQ,
	SENSOR_L_NINEQ,
	SENSOR_L_PEDQ,
	SENSOR_L_GEOMAG,
	SENSOR_L_PRESSURE,
	SENSOR_L_GYRO_CAL,
	SENSOR_L_MAG_CAL,
	SENSOR_L_EIS_GYRO,
	/*wake sensors */
	SENSOR_L_ACCEL_WAKE = 13,
	SENSOR_L_GYRO_WAKE,
	SENSOR_L_MAG_WAKE,
	SENSOR_L_ALS_WAKE,
	SENSOR_L_SIXQ_WAKE,
	SENSOR_L_NINEQ_WAKE,
	SENSOR_L_PEDQ_WAKE,
	SENSOR_L_GEOMAG_WAKE,
	SENSOR_L_PRESSURE_WAKE,
	SENSOR_L_GYRO_CAL_WAKE,
	SENSOR_L_MAG_CAL_WAKE,
	SENSOR_L_GESTURE_ACCEL,
	SENSOR_L_NUM_MAX,
};

/**
 *  struct android_l_sensor - information for each android sensor.
 *  @ts: this sensors timestamp.
 *  @base: android sensor based on invensense sensor.
 *  @rate: output rate.
 *  @on:  sensor on/off.
 *  @wake_on: wake on sensor is on/off.
 *  @div: divider for the output.
 *  @counter: counter works with the divider.
 *  @header: header for the output.
 */
struct android_l_sensor {
	u64 ts;
	enum INV_SENSORS base;
	int rate;
	bool on;
	bool wake_on;
	int div;
	int counter;
	u16 header;
};

/**
 *  struct inv_batch - information for batchmode.
 *  @on: normal batch mode on.
 *  @default_on: default batch on. This is optimization option.
 *  @overflow_on: overflow mode for batchmode.
 *  @wake_fifo_on: overflow for suspend mode.
 *  @step_only: mean only step detector data is batched.
 *  @post_isr_run: mean post isr has runned once.
 *  @counter: counter for batch mode.
 *  @timeout: nominal timeout value for batchmode in milliseconds.
 *  @max_rate: max rate for all batched sensors.
 *  @pk_size: packet size;
 *  @engine_base: engine base batch mode should stick to.
 */
struct inv_batch {
	bool on;
	bool default_on;
	bool overflow_on;
	bool wake_fifo_on;
	bool step_only;
	bool post_isr_run;
	u32 counter;
	u32 timeout;
	u32 max_rate;
	u32 pk_size;
	u32 fifo_wm_th;
	enum INV_ENGINE engine_base;
};

/**
 *  struct inv_chip_config_s - Cached chip configuration data.
 *  @fsr:		Full scale range.
 *  @lpf:		Digital low pass filter frequency.
 *  @accel_fs:		accel full scale range.
 *  @accel_enable:	enable accel functionality
 *  @gyro_enable:	enable gyro functionality
 *  @compass_enable:    enable compass functinality.
 *  @geomag_enable:     enable geomag sensor functions.
 *  @als_enable:        enable ALS functionality.
 *  @pressure_enable:   eanble pressure functionality.
 *  @secondary_enable:  secondary I2C bus enabled or not.
 *  @has_gyro:	has gyro or not.
 *  @has_compass:	has secondary I2C compass or not.
 *  @has_pressure:      has secondary I2C pressure or not.
 *  @has_als:           has secondary I2C als or not.
 *  @slave_enable:      secondary I2C interface enabled or not.
 *  @normal_compass_measure: discard first compass data after reset.
 *  @is_asleep:		1 if chip is powered down.
 *  @lp_en_set:         1 if LP_EN bit is set;
 *  @lp_en_mode_off:    debug mode that turns off LP_EN mode off.
 *  @clk_sel:           debug_mode that turns on/off clock selection.
 *  @dmp_on:		dmp is on/off.
 *  @dmp_event_int_on:  dmp event interrupt on/off.
 *  @wom_on:        WOM interrupt on. This is an internal variable.
 *  @step_indicator_on: step indicate bit added to the sensor or not.
 *  @tilt_enable: tilt enable.
 *  @pick_up_enable: pick up gesture enable.
 *  @step_detector_on:  step detector on or not.
 *  @activity_on: turn on/off activity.
 *  @activity_eng_on: activity engine on/off.
 *  @firmware_loaded:	flag indicate firmware loaded or not.
 *  @low_power_gyro_on: flag indicating low power gyro on/off.
 *  @wake_on: any wake on sensor is on/off.
 *  @compass_rate:    compass engine rate. Determined by underlying data.
 */
struct inv_chip_config_s {
	u32 fsr:2;
	u32 lpf:3;
	u32 accel_fs:2;
	u32 accel_enable:1;
	u32 gyro_enable:1;
	u32 compass_enable:1;
	u32 geomag_enable:1;
	u32 als_enable:1;
	u32 prox_enable:1;
	u32 pressure_enable:1;
	u32 has_gyro:1;
	u32 has_compass:1;
	u32 has_pressure:1;
	u32 has_als:1;
	u32 slave_enable:1;
	u32 normal_compass_measure:1;
	u32 is_asleep:1;
	u32 lp_en_set:1;
	u32 lp_en_mode_off:1;
	u32 clk_sel:1;
	u32 dmp_on:1;
	u32 dmp_event_int_on:1;
	u32 wom_on:1;
	u32 step_indicator_on:1;
	u32 tilt_enable:1;
	u32 pick_up_enable:1;
	u32 eis_enable:1;
	u32 step_detector_on:1;
	u32 activity_on:1;
	u32 activity_eng_on:1;
	u32 firmware_loaded:1;
	u32 low_power_gyro_on:1;
	u32 wake_on:1;
	int compass_rate;
};

/**
 *  struct inv_temp_comp - temperature compensation structure.
 *  @t_lo:    raw temperature in low temperature.
 *  @t_hi:    raw temperature in high temperature.
 *  @b_lo:    gyro bias in low temperature.
 *  @b_hi:    gyro bias in high temperature.
 *  @has_low:    flag indicate low temperature parameters is updated.
 *  @has_high:   flag indicates high temperature parameters is updated.
 *  @slope:      slope for temperature compensation.
 */
struct inv_temp_comp {
	int t_lo;
	int t_hi;
	int b_lo[3];
	int b_hi[3];
	bool has_low;
	bool has_high;
	int slope[3];
};

/**
 *  struct inv_chip_info_s - Chip related information.
 *  @product_id:	Product id.
 *  @product_revision:	Product revision.
 *  @silicon_revision:	Silicon revision.
 *  @software_revision:	software revision.
 *  @compass_sens:	compass sensitivity.
 *  @gyro_sens_trim:	Gyro sensitivity trim factor.
 *  @accel_sens_trim:    accel sensitivity trim factor.
 */
struct inv_chip_info_s {
	u8 product_id;
	u8 product_revision;
	u8 silicon_revision;
	u8 software_revision;
	u8 compass_sens[3];
	u32 gyro_sens_trim;
	u32 accel_sens_trim;
};

/**
 * struct inv_smd significant motion detection structure.
 * @threshold: accel threshold for motion detection.
 * @delay: delay time to confirm 2nd motion.
 * @delay2: delay window parameter.
 * @on: smd on/off.
 */
struct inv_smd {
	u32 threshold;
	u32 delay;
	u32 delay2;
	bool on;
};

/**
 * struct inv_ped pedometer related data structure.
 * @step: steps taken.
 * @time: time taken during the period.
 * @last_step_time: last time the step is taken.
 * @step_thresh: step threshold to show steps.
 * @int_thresh: step threshold to generate interrupt.
 * @int_on:   pedometer interrupt enable/disable.
 * @on:  pedometer on/off.
 * @engine_on: pedometer engine on/off.
 */
struct inv_ped {
	u64 step;
	u64 time;
	u64 last_step_time;
	u16 step_thresh;
	u16 int_thresh;
	bool int_on;
	bool on;
	bool engine_on;
};

/**
 * struct inv_eis EIS related data structure.
 * @prev_gyro: latest gyro data just before FSYNC triggerd
 * @prev_timestamp: latest gyro timestamp just before FSYNC triggered
 * @current_gyro: gyro data just after FSYNC triggerd
 * @current_timestamp: gyro timestamp just after FSYNC triggered
 * @fsync_timestamp: timestamp of FSYNC event
 * @fsync_delay: delay time of FSYNC and Gyro data. DMP data of FSYNC event
 * @eis_triggered: check fsync event is triggered or not.
 * @eis_frame: current frame is eis frame;
 * @current_sync: current frame contains fsync counter.
 * @frame_count: frame count for synchronization.
 */
struct inv_eis {
	int prev_gyro[3];
	u64 prev_timestamp;
	int current_gyro[3];
	u64 current_timestamp;
	u32 frame_dur;
	u64 slope[3];
	u64 fsync_timestamp;
	u64 last_fsync_timestamp;
	u16 fsync_delay;
	bool eis_triggered;
	bool eis_frame;
	bool current_sync;
	bool prev_state;
	u32 frame_count;
	int gyro_counter;
	int gyro_counter_s[3];
	int fsync_delay_s[3];
	int voting_count;
	int voting_count_sub;
	int voting_state;
	int count_precision;
};

enum TRIGGER_STATE {
	DATA_TRIGGER = 0,
	RATE_TRIGGER,
	EVENT_TRIGGER,
	MISC_TRIGGER,
	DEBUG_TRIGGER,
};

enum inv_fifo_count_mode {
	BYTE_MODE,
	RECORD_MODE
};

/**
 *  struct inv_secondary_reg - secondary registers data structure.
 *  @addr:       address of the slave.
 *  @reg: register address of slave.
 *  @ctrl: control register.
 *  @d0: data out register.
 */
struct inv_secondary_reg {
	u8 addr;
	u8 reg;
	u8 ctrl;
	u8 d0;
};

struct inv_secondary_set {
	u8 delay_enable;
	u8 delay_time;
	u8 odr_config;
};
/**
 *  struct inv_engine_info - data structure for engines.
 *  @base_time: base time for each engine.
 *  @base_time_1k: base time when chip is running at 1K;
 *  @divider: divider used to downsample engine rate from original rate.
 *  @running_rate: the actually running rate of engine.
 *  @orig_rate: original rate for each engine before downsample.
 *  @dur: duration for one tick.
 *  @last_update_time: last update time.
 */
struct inv_engine_info {
	u32 base_time;
	u32 base_time_1k;
	u32 divider;
	u32 running_rate;
	u32 orig_rate;
	u32 dur;
	u64 last_update_time;
};

struct inv_ois {
	int gyro_fs;
	int accel_fs;
	bool en;
};

/**
 *  struct inv_timestamp_algo - timestamp algorithm .
 *  @last_run_time: last time the post ISR runs.
 *  @ts_for_calib: ts storage for calibration.
 *  @reset_ts: reset time.
 *  @dmp_ticks: dmp ticks storage for calibration.
 *  @start_dmp_counter: dmp counter when start a new session.
 *  @calib_counter: calibration counter for timestamp.
 *  @resume_flag: flag to indicate this is the first time after resume. time
                 could have up to 1 seconds difference.
 *  @clock_base: clock base to calculate the timestamp.
 *  @gyro_ts_shift: 9 K counter for EIS.
 *  @first_sample: first of 1K running should be dropped it affects timing
 */
struct inv_timestamp_algo {
	u64 last_run_time;
	u64 ts_for_calib;
	u64 reset_ts;
	u32 dmp_ticks;
	u32 start_dmp_counter;
	int calib_counter;
	bool resume_flag;
	enum INV_ENGINE clock_base;
	u32 gyro_ts_shift;
	u32 first_sample;
};

struct inv_mpu_slave;
/**
 *  struct inv_mpu_state - Driver state variables.
 *  @dev:               device address of the current bus, i2c or spi.
 *  @chip_config:	Cached attribute information.
 *  @chip_info:		Chip information from read-only registers.
 *  @smd:               SMD data structure.
 *  @ped:               pedometer data structure.
 *  @batch:             batchmode data structure.
 *  @temp_comp:         gyro temperature compensation structure.
 *  @slave_compass:     slave compass.
 *  @slave_pressure:    slave pressure.
 *  @slave_als:         slave als.
 *  @slv_reg: slave register data structure.
 *  @ts_algo: timestamp algorithm data structure.
 *  @sec_set: slave register odr config.
 *  @eng_info: information for each engine.
 *  @hw:		Other hardware-specific information.
 *  @chip_type:		chip type.
 *  @suspend_resume_sema: semaphore for suspend/resume.
 *  @wake_lock: wake lock of the system.
 *  @client:		i2c client handle.
 *  @plat_data:		platform data.
 *  @sl_handle:         Handle to I2C port.
 *  @sensor{SENSOR_NUM_MAX]: sensor individual properties.
 *  @sensor_l[SENSOR_L_NUM_MAX]: android L sensors properties.
 *  @sensor_accuracy[SENSOR_ACCURACY_NUM_MAX]: sensor accuracy.
 *  @sensor_acurracy_flag: flag indiciate whether to check output accuracy.
 *  @irq:               irq number store.
 *  @accel_bias:        accel bias store.
 *  @gyro_bias:         gyro bias store.
 *  @accel_st_bias:     accel bias store, result of self-test.
 *  @gyro_st_bias:      gyro bias store, result of self-test.
 *  @gyro_ois_st_bias:  gyro bias store from ois self test result.
 *  @input_accel_dmp_bias[3]: accel bias for dmp.
 *  @input_gyro_dmp_bias[3]: gyro bias for dmp.
 *  @input_compass_dmp_bias[3]: compass bias for dmp.
 *  @input_accel_bias[3]: accel bias for offset register.
 *  @input_gyro_bias[3]: gyro bias for offset register.
 *  @fifo_data[8]: fifo data storage.
 *  @i2c_addr:          i2c address.
 *  @header_count:      header count in current FIFO.
 *  @step_det_count:    number of step detectors in one batch.
 *  @gyro_sf: gyro scale factor.
 *  @left_over[LEFT_OVER_BYTES]: left over bytes storage.
 *  @left_over_size: left over size.
 *  @fifo_count: current fifo_count;
 *  @wake_sensor_received: wake up sensor received.
 *  @accel_cal_enable:  accel calibration on/off
 *  @gyro_cal_enable:   gyro calibration on/off
 *  @calib_compass_on: calibrate compass on.
 *  @debug_determine_engine_on: determine engine on/off.
 *  @poke_mode_on: poke mode on/off.
 *  @mode_1k_on: indicate 1K Hz mode is on.
 *  @poke_ts: time stamp for poke feature.
 *  @step_detector_base_ts: base time stamp for step detector calculation.
 *  @last_temp_comp_time: last time temperature compensation is done.
 *  @i2c_dis: disable I2C interface or not.
 *  @name: name for the chip.
 *  @gyro_st_data: gyro self test data.
 *  @accel_st_data: accel self test data.
 *  @secondary_name: name for the slave device in the secondary I2C.
 *  @compass_var: compass variance from DMP.
 *  @current_compass_matrix: matrix compass data multiplied to before soft iron.
 *  @final_compass_matrix: matrix compass data multiplied to before soft iron.
 *  @trigger_state: information that which part triggers set_inv_enable.
 *  @firmware: firmware data pointer.
 *  @accel_calib_threshold: accel calibration threshold;
 *  @accel_calib_rate: divider for accel calibration rate.
 *  @accel_covariance[COVARIANCE_SIZE]: accel covariance data;
 *  @kf: kfifo for activity store.
 *  @activity_size: size for activity.
 *  @cntl: control word for sensor enable.
 *  @cntl2: control word for sensor extension.
 *  @motion_event_cntl: control word for events.
 *  @dmp_image_size: dmp image size.
 *  @dmp_start_address: start address of dmp.
 *  @step_counter_l_on: step counter android L sensor on/off.
 *  @step_counter_wake_l_on: step counter android L sensor wake on/off .
 *  @step_detector_l_on: step detector android L sensor on/off.
 *  @step_detector_wake_l_on: step detector android L sensor wake on/off .
 *  @gesture_only_on: indicate it is gesture only.
 *  @mag_divider: mag divider when gyro/accel is faster than mag maximum rate.
 *  @special_mag_mode: for 20690, there is special mag mode need to be handled.
 *  @mag_start_flag: when mag divider is non zero, need to check the start.
 *  @prev_steps: previous steps sent to the user.
 *  @aut_key_in: authentication key input.
 *  @aut_key_out: authentication key output.
 *  @suspend_state: state variable to indicate that we are in suspend state.
 *  @secondary_gyro_on: DMP out signal to turn on gyro.
 *  @secondary_mag_on:  DMP out signal to turn on mag.
 *  @secondary_prox_on: DMP out signal to turn on proximity.
 *  @secondary_switch: showing this setup is triggerred by secondary switch.
 *  @send_calib_gyro:       flag to indicate to send calibrated gyro.
 *  @send_raw_compass: flag to send raw compass.
 *  @resume_state: flag to synchronize the processing of inv_read_fifo()
 *  @cycle_on: variable indicate accel cycle mode is on.
 *  @secondary_switch_data: secondary switch data for activity.
 *  @raw_gyro_data[6]:    save raw gyro data.
 *  @raw_compass_data[3]: save raw compass data.
 *  @wait_queue: wait queue to wake up inv_read_fifo()
 *  @bac_drive_conf: bac drive configuration.
 *  @bac_walk_conf: bac walk configuration.
 *  @bac_smd_conf: bac smd configuration.
 *  @bac_bike_conf: bac bike configuration.
 *  @bac_run_conf: bac run configuration.
 *  @bac_still_conf: back still configuration.
 *  @power_on_data: power on data.
 *  @fifo_data_store: store of FIFO data.
 *  @int_en: store interrupt enable register data.
 *  @int_en2: store interrupt enable register 2 data.
 *  @gesture_int_count: interrupt count for gesture only mode.
 *  @smplrt_div: SMPLRT_DIV register value.
 */
struct inv_mpu_state {
	struct device *dev;
	int (*write)(struct inv_mpu_state *st, u8 reg, u8 data);
	int (*read)(struct inv_mpu_state *st, u8 reg, int len, u8 *data);
	int (*mem_write)(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
	                 u32 len, u8 const *data);
	int (*mem_read)(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
	                u32 len, u8 *data);
	struct inv_chip_config_s chip_config;
	struct inv_chip_info_s chip_info;
	struct inv_smd smd;
	struct inv_ped ped;
	struct inv_eis eis;
	struct inv_batch batch;
	struct inv_temp_comp temp_comp;
	struct inv_mpu_slave *slave_compass;
	struct inv_mpu_slave *slave_pressure;
	struct inv_mpu_slave *slave_als;
	struct inv_secondary_reg slv_reg[4];
	struct inv_timestamp_algo ts_algo;
	struct inv_secondary_set sec_set;
	struct inv_engine_info eng_info[ENGINE_NUM_MAX];
	const struct inv_hw_s *hw;
	enum inv_devices chip_type;
	enum inv_bus_type bus_type;
	enum inv_fifo_count_mode fifo_count_mode;
#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock wake_lock;
#else
	struct wakeup_source wake_lock;
#endif
#ifdef TIMER_BASED_BATCHING
	struct hrtimer hr_batch_timer;
	u64 batch_timeout;
	bool is_batch_timer_running;
	struct work_struct batch_work;
#endif
	struct i2c_client *client;
	struct mpu_platform_data plat_data;
	void *sl_handle;
	struct inv_sensor sensor[SENSOR_NUM_MAX];
	struct android_l_sensor sensor_l[SENSOR_L_NUM_MAX];
	struct inv_sensor_accuracy sensor_accuracy[SENSOR_ACCURACY_NUM_MAX];
	struct inv_ois ois;
	bool sensor_acurracy_flag[SENSOR_ACCURACY_NUM_MAX];
	short irq;
	int accel_bias[3];
	int gyro_bias[3];
	int accel_st_bias[3];
	int accel_ois_st_bias[3];
	int gyro_st_bias[3];
	int gyro_ois_st_bias[3];
	int input_accel_dmp_bias[3];
	int input_gyro_dmp_bias[3];
	int input_compass_dmp_bias[3];
	int input_accel_bias[3];
	int input_gyro_bias[3];
	u8 fifo_data[8];
	u8 i2c_addr;
	int header_count;
	int step_det_count;
	s32 gyro_sf;
	u8 left_over[LEFT_OVER_BYTES];
	u32 left_over_size;
	u32 fifo_count;
	bool wake_sensor_received;
	bool accel_cal_enable;
	bool gyro_cal_enable;
	bool calib_compass_on;
	bool debug_determine_engine_on;
	bool poke_mode_on;
	bool mode_1k_on;
	u64 poke_ts;
	u64 step_detector_base_ts;
	u64 last_temp_comp_time;
	u8 i2c_dis;
	u8 name[20];
	u8 gyro_st_data[3];
	u8 accel_st_data[3];
	u8 secondary_name[20];
	s32 compass_var;
	int current_compass_matrix[9];
	int final_compass_matrix[9];
	enum TRIGGER_STATE trigger_state;
	u8 *firmware;
	int accel_calib_threshold;
	int accel_calib_rate;
	u32 accel_covariance[COVARIANCE_SIZE];
	 DECLARE_KFIFO(kf, u8, 128);
	u32 activity_size;
	int wom_thld;
	u16 cntl;
	u16 cntl2;
	u16 motion_event_cntl;
	int dmp_image_size;
	int dmp_start_address;
	bool step_counter_l_on;
	bool step_counter_wake_l_on;
	bool step_detector_l_on;
	bool step_detector_wake_l_on;
	bool gesture_only_on;
	bool mag_start_flag;
	int mag_divider;
	bool special_mag_mode;
	int prev_steps;
	u32 curr_steps;
	int aut_key_in;
	int aut_key_out;
	bool secondary_gyro_on;
	bool secondary_mag_on;
	bool secondary_prox_on;
	bool secondary_switch;
	bool send_calib_gyro;
	bool send_raw_compass;
	bool send_raw_gyro;
	bool resume_state;
	bool cycle_on;
	int secondary_switch_data;
	u8 raw_gyro_data[6];
	u32 raw_compass_data[3];
	wait_queue_head_t wait_queue;
	u32 bac_drive_conf;
	u32 bac_walk_conf;
	u32 bac_smd_conf;
	u32 bac_bike_conf;
	u32 bac_run_conf;
	u32 bac_still_conf;
	u32 power_on_data;
	u8 fifo_data_store[HARDWARE_FIFO_SIZE + LEFT_OVER_BYTES];
	u8 int_en;
	u8 int_en_2;
	u8 gesture_int_count;
	u8 smplrt_div;
#ifdef CONFIG_ENABLE_IAM_ACC_GYRO_BUFFERING
	bool read_acc_boot_sample;
	bool read_gyro_boot_sample;
	int acc_bufsample_cnt;
	int gyro_bufsample_cnt;
	bool acc_buffer_inv_samples;
	bool gyro_buffer_inv_samples;
	struct kmem_cache *inv_acc_cachepool;
	struct kmem_cache *inv_gyro_cachepool;
	struct inv_acc_sample *inv_acc_samplist[INV_ACC_MAXSAMPLE];
	struct inv_gyro_sample *inv_gyro_samplist[INV_GYRO_MAXSAMPLE];
	ktime_t timestamp;
	int max_buffer_time;
	struct input_dev *accbuf_dev;
	struct input_dev *gyrobuf_dev;
	int report_evt_cnt;
#endif

};

/**
 *  struct inv_mpu_slave - MPU slave structure.
 *  @st_upper:  compass self test upper limit.
 *  @st_lower:  compass self test lower limit.
 *  @scale: compass scale.
 *  @rate_scale: decide how fast a compass can read.
 *  @min_read_time: minimum time between each reading.
 *  @self_test: self test method of the slave.
 *  @set_scale: set scale of slave
 *  @get_scale: read scale back of the slave.
 *  @suspend:		suspend operation.
 *  @resume:		resume operation.
 *  @setup:		setup chip. initialization.
 *  @combine_data:	combine raw data into meaningful data.
 *  @read_data:        read external sensor and output
 *  @get_mode:		get current chip mode.
 *  @set_lpf:            set low pass filter.
 *  @set_fs:             set full scale
 *  @prev_ts: last time it is read.
 */
struct inv_mpu_slave {
	const short *st_upper;
	const short *st_lower;
	int scale;
	int rate_scale;
	int min_read_time;
	int (*self_test) (struct inv_mpu_state *);
	int (*set_scale) (struct inv_mpu_state *, int scale);
	int (*get_scale) (struct inv_mpu_state *, int *val);
	int (*suspend) (struct inv_mpu_state *);
	int (*resume) (struct inv_mpu_state *);
	int (*setup) (struct inv_mpu_state *);
	int (*combine_data) (u8 *in, short *out);
	int (*read_data) (struct inv_mpu_state *, short *out);
	int (*get_mode) (void);
	int (*set_lpf) (struct inv_mpu_state *, int rate);
	int (*set_fs) (struct inv_mpu_state *, int fs);
	u64 prev_ts;
};

/* scan element definition */
enum inv_mpu_scan {
	INV_MPU_SCAN_TIMESTAMP,
};

/* IIO attribute address */
enum MPU_IIO_ATTR_ADDR {
	ATTR_DMP_GYRO_X_DMP_BIAS,
	ATTR_DMP_GYRO_Y_DMP_BIAS,
	ATTR_DMP_GYRO_Z_DMP_BIAS,
	ATTR_DMP_GYRO_CAL_ENABLE,
	ATTR_DMP_ACCEL_X_DMP_BIAS,
	ATTR_DMP_ACCEL_Y_DMP_BIAS,
	ATTR_DMP_ACCEL_Z_DMP_BIAS,
	ATTR_DMP_MAGN_X_DMP_BIAS,
	ATTR_DMP_MAGN_Y_DMP_BIAS,
	ATTR_DMP_MAGN_Z_DMP_BIAS,
	ATTR_DMP_MAGN_ACCURACY,
	ATTR_GYRO_X_OFFSET,
	ATTR_GYRO_Y_OFFSET,
	ATTR_GYRO_Z_OFFSET,
	ATTR_ACCEL_X_OFFSET,
	ATTR_ACCEL_Y_OFFSET,
	ATTR_ACCEL_Z_OFFSET,
	ATTR_DMP_SC_AUTH,
	ATTR_DMP_EIS_AUTH,
	ATTR_DMP_ACCEL_CAL_ENABLE,
	ATTR_DMP_PED_INT_ON,
	ATTR_DMP_PED_STEP_THRESH,
	ATTR_DMP_PED_INT_THRESH,
	ATTR_DMP_PED_ON,
	ATTR_DMP_SMD_ENABLE,
	ATTR_DMP_TILT_ENABLE,
	ATTR_DMP_PICK_UP_ENABLE,
	ATTR_DMP_EIS_ENABLE,
	ATTR_DMP_PEDOMETER_STEPS,
	ATTR_DMP_PEDOMETER_TIME,
	ATTR_DMP_PEDOMETER_COUNTER,
	ATTR_DMP_LOW_POWER_GYRO_ON,
	ATTR_DMP_LP_EN_OFF,
	ATTR_DMP_CLK_SEL,
	ATTR_DMP_DEBUG_MEM_READ,
	ATTR_DMP_DEBUG_MEM_WRITE,
	ATTR_DEBUG_REG_WRITE,
	ATTR_DEBUG_WRITE_CFG,
	ATTR_DEBUG_REG_ADDR,
	ATTR_WOM_THLD,
	/* *****above this line, are DMP features, power needs on/off */
	/* *****below this line, are DMP features, no power needed */
	ATTR_IN_POWER_ON,
	ATTR_DMP_ON,
	ATTR_DMP_EVENT_INT_ON,
	ATTR_DMP_STEP_COUNTER_ON,
	ATTR_DMP_STEP_COUNTER_WAKE_ON,
	ATTR_DMP_BATCHMODE_TIMEOUT,
	ATTR_DMP_BATCHMODE_WAKE_FIFO_FULL,
	ATTR_DMP_STEP_DETECTOR_ON,
	ATTR_DMP_STEP_DETECTOR_WAKE_ON,
	ATTR_DMP_ACTIVITY_ON,
	ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE,
	ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE,
	ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON,
	ATTR_DMP_MISC_GYRO_RECALIBRATION,
	ATTR_DMP_MISC_ACCEL_RECALIBRATION,
	ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD,
	ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE,
	ATTR_GYRO_SCALE,
	ATTR_ACCEL_SCALE,
	ATTR_COMPASS_SCALE,
	ATTR_COMPASS_SENSITIVITY_X,
	ATTR_COMPASS_SENSITIVITY_Y,
	ATTR_COMPASS_SENSITIVITY_Z,
	ATTR_GYRO_ENABLE,
	ATTR_ACCEL_ENABLE,
	ATTR_COMPASS_ENABLE,
	ATTR_FIRMWARE_LOADED,
	ATTR_POKE_MODE,
	ATTR_ANGLVEL_X_CALIBBIAS,
	ATTR_ANGLVEL_Y_CALIBBIAS,
	ATTR_ANGLVEL_Z_CALIBBIAS,
	ATTR_ACCEL_X_CALIBBIAS,
	ATTR_ACCEL_Y_CALIBBIAS,
	ATTR_ACCEL_Z_CALIBBIAS,
	ATTR_ANGLVEL_X_ST_CALIBBIAS,
	ATTR_ANGLVEL_Y_ST_CALIBBIAS,
	ATTR_ANGLVEL_Z_ST_CALIBBIAS,
	ATTR_ANGLVEL_X_OIS_ST_CALIBBIAS,
	ATTR_ANGLVEL_Y_OIS_ST_CALIBBIAS,
	ATTR_ANGLVEL_Z_OIS_ST_CALIBBIAS,
	ATTR_ACCEL_X_ST_CALIBBIAS,
	ATTR_ACCEL_Y_ST_CALIBBIAS,
	ATTR_ACCEL_Z_ST_CALIBBIAS,
	ATTR_ACCEL_X_OIS_ST_CALIBBIAS,
	ATTR_ACCEL_Y_OIS_ST_CALIBBIAS,
	ATTR_ACCEL_Z_OIS_ST_CALIBBIAS,
	ATTR_GYRO_MATRIX,
	ATTR_ACCEL_MATRIX,
	ATTR_COMPASS_MATRIX,
	ATTR_FSYNC_FRAME_COUNT,
	ATTR_SECONDARY_NAME,
	ATTR_GYRO_SF,
	ATTR_BAC_DRIVE_CONFIDENCE,
	ATTR_BAC_WALK_CONFIDENCE,
	ATTR_BAC_SMD_CONFIDENCE,
	ATTR_BAC_BIKE_CONFIDENCE,
	ATTR_BAC_STILL_CONFIDENCE,
	ATTR_BAC_RUN_CONFIDENCE,
	IN_OIS_ACCEL_FS,
	IN_OIS_GYRO_FS,
	IN_OIS_ENABLE,
};

int inv_mpu_configure_ring(struct iio_dev *indio_dev);
int inv_mpu_probe_trigger(struct iio_dev *indio_dev);
void inv_mpu_unconfigure_ring(struct iio_dev *indio_dev);
void inv_mpu_remove_trigger(struct iio_dev *indio_dev);
#ifdef CONFIG_PM_SLEEP
int inv_mpu_suspend(struct iio_dev *indio_dev);
void inv_mpu_complete(struct iio_dev *indio_dev);
#endif

int inv_get_pedometer_steps(struct inv_mpu_state *st, int *ped);
int inv_get_pedometer_time(struct inv_mpu_state *st, int *ped);
int inv_read_pedometer_counter(struct inv_mpu_state *st);

int inv_dmp_read(struct inv_mpu_state *st, int off, int size, u8 *buf);
int inv_firmware_load(struct inv_mpu_state *st);

int set_inv_enable(struct iio_dev *indio_dev);

int inv_mpu_setup_compass_slave(struct inv_mpu_state *st);
int inv_mpu_setup_pressure_slave(struct inv_mpu_state *st);
int inv_mpu_setup_als_slave(struct inv_mpu_state *st);
int inv_mpu_initialize(struct inv_mpu_state *st);
int inv_set_accel_sf(struct inv_mpu_state *st);
int inv_set_gyro_sf(struct inv_mpu_state *st);
s64 get_time_ns(void);
int inv_i2c_read_base(struct inv_mpu_state *st, u16 i, u8 r, u16 l, u8 *d);
int inv_i2c_single_write_base(struct inv_mpu_state *st, u16 i, u8 r, u8 d);
int write_be32_to_mem(struct inv_mpu_state *st, u32 data, int addr);
int write_be16_to_mem(struct inv_mpu_state *st, u16 data, int addr);
int read_be32_from_mem(struct inv_mpu_state *st, u32 *o, int addr);
int read_be16_from_mem(struct inv_mpu_state *st, u16 *o, int addr);
u32 inv_get_cntr_diff(u32 curr_counter, u32 prev);
int inv_write_2bytes(struct inv_mpu_state *st, int k, int data);
int inv_set_bank(struct inv_mpu_state *st, u8 bank);
int inv_set_power(struct inv_mpu_state *st, bool power_on);
int inv_switch_power_in_lp(struct inv_mpu_state *st, bool on);
#ifndef CONFIG_INV_MPU_IIO_ICM20608D
int inv_set_accel_config2(struct inv_mpu_state *st, bool cycle_mode);
#endif
int inv_stop_dmp(struct inv_mpu_state *st);
int inv_reset_fifo(struct inv_mpu_state *st, bool turn_off);
int inv_create_dmp_sysfs(struct iio_dev *ind);
int inv_check_chip_type(struct iio_dev *indio_dev, const char *name);
int inv_write_compass_matrix(struct inv_mpu_state *st, int *adj);
irqreturn_t inv_read_fifo(int irq, void *dev_id);
#ifdef TIMER_BASED_BATCHING
void inv_batch_work(struct work_struct *work);
#endif
int inv_flush_batch_data(struct iio_dev *indio_dev, int data);
static inline int mpu_memory_write(struct inv_mpu_state *st, u8 mpu_addr,
                                   u16 mem_addr, u32 len, u8 const *data)
{
	int ret = -1;

	if (st->mem_write)
		ret = st->mem_write(st, mpu_addr, mem_addr, len, data);

	return ret;
}
static inline int mpu_memory_read(struct inv_mpu_state *st, u8 mpu_addr,
                                  u16 mem_addr, u32 len, u8 *data)
{
	int ret = -1;

	if (st->mem_read)
		ret = st->mem_read(st, mpu_addr, mem_addr, len, data);

	return ret;
}
int inv_read_secondary(struct inv_mpu_state *st, int ind, int addr,
			int reg, int len);
int inv_write_secondary(struct inv_mpu_state *st, int ind, int addr,
			int reg, int v);
int inv_execute_write_secondary(struct inv_mpu_state *st, int ind, int addr,
				int reg, int v);
int inv_execute_read_secondary(struct inv_mpu_state *st, int ind, int addr,
			       int reg, int len, u8 *d);

int inv_push_16bytes_buffer(struct inv_mpu_state *st, u16 hdr,
						u64 t, int *q, s16 accur);
int inv_push_gyro_data(struct inv_mpu_state *st, s16 *raw, s32 *calib, u64 t);
int inv_push_8bytes_buffer(struct inv_mpu_state *st, u16 hdr, u64 t, s16 *d);
int inv_push_8bytes_kf(struct inv_mpu_state *st, u16 hdr, u64 t, s16 *d);

void inv_push_step_indicator(struct inv_mpu_state *st, u64 t);
int inv_send_steps(struct inv_mpu_state *st, int step, u64 t);
int inv_push_marker_to_buffer(struct inv_mpu_state *st, u16 hdr, int data);

int inv_check_sensor_on(struct inv_mpu_state *st);
int inv_write_cntl(struct inv_mpu_state *st, u16 wd, bool en, int cntl);

int inv_get_packet_size(struct inv_mpu_state *st, u16 hdr,
						u32 *pk_size, u8 *dptr);
int inv_parse_packet(struct inv_mpu_state *st, u16 hdr, u8 *dptr);
int inv_pre_parse_packet(struct inv_mpu_state *st, u16 hdr, u8 *dptr);
int inv_process_dmp_data(struct inv_mpu_state *st);

int be32_to_int(u8 *d);
void inv_convert_and_push_16bytes(struct inv_mpu_state *st, u16 hdr,
							u8 *d, u64 t, s8 *m);
void inv_convert_and_push_8bytes(struct inv_mpu_state *st, u16 hdr,
						u8 *d, u64 t, s8 *m);
int inv_get_dmp_ts(struct inv_mpu_state *st, int i);
int inv_process_step_det(struct inv_mpu_state *st, u8 *dptr);
int inv_process_eis(struct inv_mpu_state *st, u16 delay);
int inv_rate_convert(struct inv_mpu_state *st, int ind, int data);

int inv_setup_dmp_firmware(struct inv_mpu_state *st);
/* used to print i2c data using pr_debug */
char *wr_pr_debug_begin(u8 const *data, u32 len, char *string);
char *wr_pr_debug_end(char *string);

int inv_hw_self_test(struct inv_mpu_state *st);
int inv_q30_mult(int a, int b);
#ifdef ACCEL_BIAS_TEST
int inv_get_3axis_average(s16 src[], s16 dst[], s16 reset);
#endif

static inline int inv_plat_single_write(struct inv_mpu_state *st,
							u8 reg, u8 data)
{
	int ret = -1;

	if (st->write)
		ret = st->write(st, reg, data);

	return ret;
}
static inline int inv_plat_read(struct inv_mpu_state *st, u8 reg,
							int len, u8 *data)
{
	int ret = -1;

	if (st->read)
		ret = st->read(st, reg, len, data);

	return ret;
}
irqreturn_t inv_read_fifo(int , void *);

int inv_stop_interrupt(struct inv_mpu_state *st);
int inv_reenable_interrupt(struct inv_mpu_state *st);

int inv_enable_pedometer_interrupt(struct inv_mpu_state *st, bool en);
int inv_dataout_control1(struct inv_mpu_state *st, u16 cntl1);
int inv_dataout_control2(struct inv_mpu_state *st, u16 cntl2);
int inv_motion_interrupt_control(struct inv_mpu_state *st,
						u16 motion_event_cntl);

int inv_bound_timestamp(struct inv_mpu_state *st);
int inv_update_dmp_ts(struct inv_mpu_state *st, int ind);
int inv_get_last_run_time_non_dmp_record_mode(struct inv_mpu_state *st);

#define mem_w(a, b, c) mpu_memory_write(st, st->i2c_addr, a, b, c)
#define mem_r(a, b, c) mpu_memory_read(st, st->i2c_addr, a, b, c)

#endif /* #ifndef _INV_MPU_IIO_H_ */
