/*!
 * @section LICENSE
 * (C) Copyright 2011~2015 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160_core.c
 * @date     2015/11/17 14:40
 * @id       "128af5d"
 * @version  1.2
 *
 * @brief
 * The core code of BMI160 device driver
 *
 * @detail
 * This file implements the core code of BMI160 device driver,
 * which includes hardware related functions, input device register,
 * device attribute files, etc.
*/


#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/math64.h>
#include "bmi160_core.h"
#include "bmi160.h"

#include <linux/input.h>
#include <linux/ktime.h>

#define DRIVER_VERSION "0.0.33.0"
#define I2C_BURST_READ_MAX_LEN      (256)
#define BMI160_STORE_COUNT  (6000)
#define LMADA 1
#ifdef BMI160_DEBUG
uint8_t debug_level;
#endif

#ifdef BMI160_DEBUG
#define LOGDI(...) { if (debug_level & 0x08) \
				dev_info(__VA_ARGS__); }
#else
#define LOGDI(...)
#endif

enum BMI_SENSOR_INT_T {
	/* Interrupt enable0*/
	BMI_ANYMO_X_INT = 0,
	BMI_ANYMO_Y_INT,
	BMI_ANYMO_Z_INT,
	BMI_D_TAP_INT,
	BMI_S_TAP_INT,
	BMI_ORIENT_INT,
	BMI_FLAT_INT,
	/* Interrupt enable1*/
	BMI_HIGH_X_INT,
	BMI_HIGH_Y_INT,
	BMI_HIGH_Z_INT,
	BMI_LOW_INT,
	BMI_DRDY_INT,
	BMI_FFULL_INT,
	BMI_FWM_INT,
	/* Interrupt enable2 */
	BMI_NOMOTION_X_INT,
	BMI_NOMOTION_Y_INT,
	BMI_NOMOTION_Z_INT,
	BMI_STEP_DETECTOR_INT,
	INT_TYPE_MAX
};

/*bmi fifo sensor type combination*/
enum BMI_SENSOR_FIFO_COMBINATION {
	BMI_FIFO_A = 0,
	BMI_FIFO_G,
	BMI_FIFO_M,
	BMI_FIFO_G_A,
	BMI_FIFO_M_A,
	BMI_FIFO_M_G,
	BMI_FIFO_M_G_A,
	BMI_FIFO_COM_MAX
};

/*bmi fifo analyse return err status*/
enum BMI_FIFO_ANALYSE_RETURN_T {
	FIFO_OVER_READ_RETURN = -10,
	FIFO_SENSORTIME_RETURN = -9,
	FIFO_SKIP_OVER_LEN = -8,
	FIFO_M_G_A_OVER_LEN = -7,
	FIFO_M_G_OVER_LEN = -6,
	FIFO_M_A_OVER_LEN = -5,
	FIFO_G_A_OVER_LEN = -4,
	FIFO_M_OVER_LEN = -3,
	FIFO_G_OVER_LEN = -2,
	FIFO_A_OVER_LEN = -1
};

/*!bmi sensor generic power mode enum */
enum BMI_DEV_OP_MODE {
	SENSOR_PM_NORMAL = 0,
	SENSOR_PM_LP1,
	SENSOR_PM_SUSPEND,
	SENSOR_PM_LP2
};

/*! bmi acc sensor power mode enum */
enum BMI_ACC_PM_TYPE {
	BMI_ACC_PM_NORMAL = 0,
	BMI_ACC_PM_LP1,
	BMI_ACC_PM_SUSPEND,
	BMI_ACC_PM_LP2,
	BMI_ACC_PM_MAX
};

/*! bmi gyro sensor power mode enum */
enum BMI_GYRO_PM_TYPE {
	BMI_GYRO_PM_NORMAL = 0,
	BMI_GYRO_PM_FAST_START,
	BMI_GYRO_PM_SUSPEND,
	BMI_GYRO_PM_MAX
};

/*! bmi mag sensor power mode enum */
enum BMI_MAG_PM_TYPE {
	BMI_MAG_PM_NORMAL = 0,
	BMI_MAG_PM_LP1,
	BMI_MAG_PM_SUSPEND,
	BMI_MAG_PM_LP2,
	BMI_MAG_PM_MAX
};

/*! bmi sensor support type*/
enum BMI_SENSOR_TYPE {
	BMI_ACC_SENSOR,
	BMI_GYRO_SENSOR,
	BMI_MAG_SENSOR,
	BMI_SENSOR_TYPE_MAX
};

/*!bmi sensor generic power mode enum */
enum BMI_AXIS_TYPE {
	X_AXIS = 0,
	Y_AXIS,
	Z_AXIS,
	AXIS_MAX
};

/*!bmi sensor generic intterrupt enum */
enum BMI_INT_TYPE {
	BMI160_INT0 = 0,
	BMI160_INT1,
	BMI160_INT_MAX
};

/*! bmi sensor time resolution definition*/
enum BMI_SENSOR_TIME_RS_TYPE {
	TS_0_78_HZ = 1,/*0.78HZ*/
	TS_1_56_HZ,/*1.56HZ*/
	TS_3_125_HZ,/*3.125HZ*/
	TS_6_25_HZ,/*6.25HZ*/
	TS_12_5_HZ,/*12.5HZ*/
	TS_25_HZ,/*25HZ, odr=6*/
	TS_50_HZ,/*50HZ*/
	TS_100_HZ,/*100HZ*/
	TS_200_HZ,/*200HZ*/
	TS_400_HZ,/*400HZ*/
	TS_800_HZ,/*800HZ*/
	TS_1600_HZ,/*1600HZ*/
	TS_MAX_HZ
};

/*! bmi sensor interface mode */
enum BMI_SENSOR_IF_MODE_TYPE {
	/*primary interface:autoconfig/secondary interface off*/
	P_AUTO_S_OFF = 0,
	/*primary interface:I2C/secondary interface:OIS*/
	P_I2C_S_OIS,
	/*primary interface:autoconfig/secondary interface:Magnetometer*/
	P_AUTO_S_MAG,
	/*interface mode reseved*/
	IF_MODE_RESEVED

};

#ifdef CONFIG_ENABLE_ACC_GYRO_BUFFERING
static void store_acc_boot_sample(int x, int y, int z)
{
	struct bmi_client_data *client_data = iio_priv(accl_iio_private);

	if (false == client_data->acc_buffer_bmi160_samples)
		return;
	client_data->timestamp = ktime_get_boottime();
	if (ktime_to_timespec(client_data->timestamp).tv_sec
				<  client_data->max_buffer_time) {
		if (client_data->acc_bufsample_cnt < BMI_ACC_MAXSAMPLE) {
			client_data->bmi160_acc_samplist[client_data->
						acc_bufsample_cnt]->xyz[0] = x;
			client_data->bmi160_acc_samplist[client_data->
						acc_bufsample_cnt]->xyz[1] = y;
			client_data->bmi160_acc_samplist[client_data->
						acc_bufsample_cnt]->xyz[2] = z;
			client_data->bmi160_acc_samplist[client_data->
						acc_bufsample_cnt]->tsec =
						ktime_to_timespec(client_data
							->timestamp).tv_sec;
			client_data->bmi160_acc_samplist[client_data->
						acc_bufsample_cnt]->tnsec =
						ktime_to_timespec(client_data
							->timestamp).tv_nsec;
			client_data->acc_bufsample_cnt++;
		}
	} else {
		client_data->acc_buffer_bmi160_samples = false;
	}
}
static void store_gyro_boot_sample(int x, int y, int z)
{
	struct bmi_client_data *client_data = iio_priv(accl_iio_private);

	if (false == client_data->gyro_buffer_bmi160_samples)
		return;
	client_data->timestamp = ktime_get_boottime();
	if (ktime_to_timespec(client_data->timestamp).tv_sec
			<  client_data->max_buffer_time) {
		if (client_data->gyro_bufsample_cnt < BMI_GYRO_MAXSAMPLE) {
			client_data->bmi160_gyro_samplist[client_data->
						gyro_bufsample_cnt]->xyz[0] = x;
			client_data->bmi160_gyro_samplist[client_data->
						gyro_bufsample_cnt]->xyz[1] = y;
			client_data->bmi160_gyro_samplist[client_data->
						gyro_bufsample_cnt]->xyz[2] = z;
			client_data->bmi160_gyro_samplist[client_data->
						gyro_bufsample_cnt]->tsec =
						ktime_to_timespec(client_data->
							timestamp).tv_sec;
			client_data->bmi160_gyro_samplist[client_data->
						gyro_bufsample_cnt]->tnsec =
						ktime_to_timespec(client_data->
							timestamp).tv_nsec;
			client_data->gyro_bufsample_cnt++;
		}
	} else {
		client_data->gyro_buffer_bmi160_samples = false;
	}
}
#else
static void store_acc_boot_sample(int x, int y, int z)
{
}
static void store_gyro_boot_sample(int x, int y, int z)
{
}
#endif

#ifdef CONFIG_ENABLE_ACC_GYRO_BUFFERING
static int bmi160_acc_gyro_early_buff_init(void)
{
	int i = 0, err = 0;
	struct bmi_client_data *client_data = iio_priv(accl_iio_private);

	client_data->acc_bufsample_cnt = 0;
	client_data->gyro_bufsample_cnt = 0;
	client_data->report_evt_cnt = 5;
	client_data->max_buffer_time = 40;

	client_data->bmi_acc_cachepool = kmem_cache_create("acc_sensor_sample",
			sizeof(struct bmi_acc_sample),
			0,
			SLAB_HWCACHE_ALIGN, NULL);
	for (i = 0; i < BMI_ACC_MAXSAMPLE; i++) {
		client_data->bmi160_acc_samplist[i] =
			kmem_cache_alloc(client_data->bmi_acc_cachepool,
					GFP_KERNEL);
		if (!client_data->bmi160_acc_samplist[i]) {
			err = -ENOMEM;
			dev_err(client_data->dev,
				"slab:memory allocation failed:" "%d\n", err);
			goto clean_exit1;
		}
	}

	client_data->bmi_gyro_cachepool = kmem_cache_create("gyro_sensor_sample"
				, sizeof(struct bmi_gyro_sample), 0,
				SLAB_HWCACHE_ALIGN, NULL);
	for (i = 0; i < BMI_GYRO_MAXSAMPLE; i++) {
		client_data->bmi160_gyro_samplist[i] =
			kmem_cache_alloc(client_data->bmi_gyro_cachepool,
					GFP_KERNEL);
		if (!client_data->bmi160_gyro_samplist[i]) {
			err = -ENOMEM;
			dev_err(client_data->dev,
				"slab:memory allocation failed:" "%d\n", err);
			goto clean_exit2;
		}
	}

	client_data->accbuf_dev = input_allocate_device();
	if (!client_data->accbuf_dev) {
		err = -ENOMEM;
		dev_err(client_data->dev, "input device allocation failed\n");
		goto clean_exit3;
	}
	client_data->accbuf_dev->name = "bmi160_accbuf";
	client_data->accbuf_dev->id.bustype = BUS_I2C;
	input_set_events_per_packet(client_data->accbuf_dev,
			client_data->report_evt_cnt * BMI_ACC_MAXSAMPLE);
	set_bit(EV_ABS, client_data->accbuf_dev->evbit);
	input_set_abs_params(client_data->accbuf_dev, ABS_X,
							-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->accbuf_dev, ABS_Y,
							-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->accbuf_dev, ABS_Z,
							-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->accbuf_dev, ABS_RX,
							-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->accbuf_dev, ABS_RY,
							-G_MAX, G_MAX, 0, 0);
	err = input_register_device(client_data->accbuf_dev);
	if (err) {
		dev_err(client_data->dev,
				"unable to register input device %s\n",
				client_data->accbuf_dev->name);
		goto clean_exit3;
	}

	client_data->gyrobuf_dev = input_allocate_device();
	if (!client_data->gyrobuf_dev) {
		err = -ENOMEM;
		dev_err(client_data->dev, "input device allocation failed\n");
		goto clean_exit4;
	}
	client_data->gyrobuf_dev->name = "bmi160_gyrobuf";
	client_data->gyrobuf_dev->id.bustype = BUS_I2C;
	input_set_events_per_packet(client_data->gyrobuf_dev,
			client_data->report_evt_cnt * BMI_GYRO_MAXSAMPLE);
	set_bit(EV_ABS, client_data->gyrobuf_dev->evbit);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_X,
							-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_Y,
							-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_Z,
							-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_RX,
							-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(client_data->gyrobuf_dev, ABS_RY,
							-G_MAX, G_MAX, 0, 0);
	err = input_register_device(client_data->gyrobuf_dev);
	if (err) {
		dev_err(client_data->dev,
				"unable to register input device %s\n",
				client_data->gyrobuf_dev->name);
		goto clean_exit4;
	}

	client_data->acc_buffer_bmi160_samples = true;
	client_data->gyro_buffer_bmi160_samples = true;
	bmi160_set_command_register(ACCEL_MODE_NORMAL);
	bmi160_set_command_register(GYRO_MODE_NORMAL);
	bmi160_set_accel_output_data_rate(BMI160_ACCEL_OUTPUT_DATA_RATE_100HZ);
	bmi160_set_accel_range(BMI160_ACCEL_RANGE_2G);
	bmi160_set_gyro_output_data_rate(BMI160_GYRO_OUTPUT_DATA_RATE_100HZ);
	bmi160_set_gyro_range(BMI160_GYRO_RANGE_125_DEG_SEC);
	bmi160_set_intr_enable_1(BMI160_DATA_RDY_ENABLE, BMI160_ENABLE);
	return 1;
clean_exit4:
	input_free_device(client_data->gyrobuf_dev);
	input_unregister_device(client_data->accbuf_dev);
clean_exit3:
	input_free_device(client_data->accbuf_dev);
clean_exit2:
	for (i = 0; i < BMI_GYRO_MAXSAMPLE; i++)
		kmem_cache_free(client_data->bmi_gyro_cachepool,
				client_data->bmi160_gyro_samplist[i]);
	kmem_cache_destroy(client_data->bmi_gyro_cachepool);
clean_exit1:
	for (i = 0; i < BMI_ACC_MAXSAMPLE; i++)
		kmem_cache_free(client_data->bmi_acc_cachepool,
				client_data->bmi160_acc_samplist[i]);
	kmem_cache_destroy(client_data->bmi_acc_cachepool);
	return 0;
}
static void bmi160_acc_gyro_input_cleanup(
				struct bmi_client_data *client_data)
{
	int i = 0;

	input_free_device(client_data->accbuf_dev);
	input_unregister_device(client_data->gyrobuf_dev);
	input_free_device(client_data->gyrobuf_dev);
	for (i = 0; i < BMI_GYRO_MAXSAMPLE; i++)
		kmem_cache_free(client_data->bmi_gyro_cachepool,
				client_data->bmi160_gyro_samplist[i]);
	kmem_cache_destroy(client_data->bmi_gyro_cachepool);
	for (i = 0; i < BMI_ACC_MAXSAMPLE; i++)
		kmem_cache_free(client_data->bmi_acc_cachepool,
				client_data->bmi160_acc_samplist[i]);
	kmem_cache_destroy(client_data->bmi_acc_cachepool);
}
#else
static int bmi160_acc_gyro_early_buff_init(void)
{
	return 1;
}
static void bmi160_acc_gyro_input_cleanup(
			struct bmi_client_data *client_data)
{
}
#endif
#ifdef CONFIG_ENABLE_ACC_GYRO_BUFFERING
static inline int bmi160_check_acc_early_buff_enable_flag(
		struct bmi_client_data *client_data)
{
	if (true == client_data->acc_buffer_bmi160_samples)
		return 1;
	else
		return 0;
}
static inline int bmi160_check_gyro_early_buff_enable_flag(
		struct bmi_client_data *client_data)
{
	if (true == client_data->gyro_buffer_bmi160_samples)
		return 1;
	else
		return 0;
}
static inline int bmi160_check_acc_gyro_early_buff_enable_flag(void)
{
	struct bmi_client_data *client_data = iio_priv(accl_iio_private);

	if (true == client_data->acc_buffer_bmi160_samples ||
			true == client_data->gyro_buffer_bmi160_samples)
		return 1;
	else
		return 0;
}
#else
static inline int bmi160_check_acc_early_buff_enable_flag(
		struct bmi_client_data *client_data)
{
	return 0;
}
static inline int bmi160_check_gyro_early_buff_enable_flag(
		struct bmi_client_data *client_data)
{
	return 0;
}
static inline int bmi160_check_acc_gyro_early_buff_enable_flag(void)
{
	return 0;
}
#endif


#define BMI_TEMP_SCALE			5000
#define BMI_TEMP_OFFSET			12000

#define BMI_CHANNELS_CONFIG(device_type, si, mod, addr, bitlen) \
	{ \
		.type = device_type, \
		.modified = 1, \
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
		.scan_index = si, \
		.channel2 = mod, \
		.address = addr, \
		.scan_type = { \
			.sign = 's', \
			.realbits = bitlen, \
			.shift = 0, \
			.storagebits = bitlen, \
			.endianness = IIO_LE, \
		}, \
	}

#define BMI_BYTE_FOR_PER_AXIS_CHANNEL		2

/*iio chan spec for bmi16x IMU sensor*/
static const struct iio_chan_spec accl_iio_channels[] = {
/*acc channel*/
BMI_CHANNELS_CONFIG(IIO_ACCEL, BMI_SCAN_ACCL_X,
IIO_MOD_X, BMI160_USER_DATA_14_ACCEL_X_LSB__REG, 16),
BMI_CHANNELS_CONFIG(IIO_ACCEL, BMI_SCAN_ACCL_Y,
IIO_MOD_Y, BMI160_USER_DATA_16_ACCEL_Y_LSB__REG, 16),
BMI_CHANNELS_CONFIG(IIO_ACCEL, BMI_SCAN_ACCL_Z,
IIO_MOD_Z, BMI160_USER_DATA_18_ACCEL_Z_LSB__REG, 16),

/*ap timestamp channel*/
IIO_CHAN_SOFT_TIMESTAMP(BMI_SCAN_TIMESTAMP)

};

static const struct iio_chan_spec gyro_iio_channels[] = {
/*gyro channel*/
BMI_CHANNELS_CONFIG(IIO_ANGL_VEL, BMI_SCAN_GYRO_X,
IIO_MOD_X, BMI160_USER_DATA_8_GYRO_X_LSB__REG, 16),
BMI_CHANNELS_CONFIG(IIO_ANGL_VEL, BMI_SCAN_GYRO_Y,
IIO_MOD_Y, BMI160_USER_DATA_10_GYRO_Y_LSB__REG, 16),
BMI_CHANNELS_CONFIG(IIO_ANGL_VEL, BMI_SCAN_GYRO_Z,
IIO_MOD_Z, BMI160_USER_DATA_12_GYRO_Z_LSB__REG, 16),

/*ap timestamp channel*/
IIO_CHAN_SOFT_TIMESTAMP(BMI_SCAN_TIMESTAMP)

};

#ifdef BMI160_MAG_INTERFACE_SUPPORT

static const struct iio_chan_spec magn_iio_channels[] = {
/*mag channel*/
BMI_CHANNELS_CONFIG(IIO_MAGN, BMI_SCAN_MAGN_X,
IIO_MOD_X, BMI160_USER_DATA_MAG_X_LSB__REG, 16),
BMI_CHANNELS_CONFIG(IIO_MAGN, BMI_SCAN_MAGN_Y,
IIO_MOD_Y, BMI160_USER_DATA_MAG_Y_LSB__REG, 16),
BMI_CHANNELS_CONFIG(IIO_MAGN, BMI_SCAN_MAGN_Z,
IIO_MOD_Z, BMI160_USER_DATA_MAG_Z_LSB__REG, 16),

/*ap timestamp channel*/
IIO_CHAN_SOFT_TIMESTAMP(BMI_SCAN_TIMESTAMP)

};
#endif



static const int bmi_pmu_cmd_acc_arr[BMI_ACC_PM_MAX] = {
	/*!bmi pmu for acc normal, low power1,
	 * suspend, low power2 mode command */
	CMD_PMU_ACC_NORMAL,
	CMD_PMU_ACC_LP1,
	CMD_PMU_ACC_SUSPEND,
	CMD_PMU_ACC_LP2
};

static const int bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_MAX] = {
	/*!bmi pmu for gyro normal, fast startup,
	 * suspend mode command */
	CMD_PMU_GYRO_NORMAL,
	CMD_PMU_GYRO_FASTSTART,
	CMD_PMU_GYRO_SUSPEND
};

static const int bmi_pmu_cmd_mag_arr[BMI_MAG_PM_MAX] = {
	/*!bmi pmu for mag normal, low power1,
	 * suspend, low power2 mode command */
	CMD_PMU_MAG_NORMAL,
	CMD_PMU_MAG_LP1,
	CMD_PMU_MAG_SUSPEND,
	CMD_PMU_MAG_LP2
};

static const char *bmi_axis_name[AXIS_MAX] = {"x", "y", "z"};

static const int bmi_interrupt_type[] = {
	/*!bmi interrupt type */
	/* Interrupt enable0 , index=0~6*/
	BMI160_ANY_MOTION_X_ENABLE,
	BMI160_ANY_MOTION_Y_ENABLE,
	BMI160_ANY_MOTION_Z_ENABLE,
	BMI160_DOUBLE_TAP_ENABLE,
	BMI160_SINGLE_TAP_ENABLE,
	BMI160_ORIENT_ENABLE,
	BMI160_FLAT_ENABLE,
	/* Interrupt enable1, index=7~13*/
	BMI160_HIGH_G_X_ENABLE,
	BMI160_HIGH_G_Y_ENABLE,
	BMI160_HIGH_G_Z_ENABLE,
	BMI160_LOW_G_ENABLE,
	BMI160_FIFO_WM_ENABLE,
	BMI160_FIFO_FULL_ENABLE,
	BMI160_DATA_RDY_ENABLE,
	/* Interrupt enable2, index = 14~17*/
	BMI160_NOMOTION_X_ENABLE,
	BMI160_NOMOTION_Y_ENABLE,
	BMI160_NOMOTION_Z_ENABLE,
	BMI160_STEP_DETECTOR_EN
};

/*! bmi sensor time depend on ODR*/
struct bmi_sensor_time_odr_tbl {
	u32 ts_duration_lsb;
	u32 ts_duration_us;
	u32 ts_delat;/*sub current delat fifo_time*/
};

struct bmi160_axis_data_t {
	s16 x;
	s16 y;
	s16 z;
};

struct bmi160_type_mapping_type {

	/*! bmi16x sensor chip id */
	uint16_t chip_id;
	/*! bmi16x chip revision code */
	uint16_t revision_id;
	/*! bma2x2 sensor name */
	const char *sensor_name;
};
uint64_t get_alarm_timestamp(void)
{
	uint64_t ts_ap;
	struct timespec tmp_time;
	/*use this interface for CTS*/
	get_monotonic_boottime(&tmp_time);
	ts_ap = (uint64_t)tmp_time.tv_sec * 1000000 + tmp_time.tv_nsec / 1000;
	return ts_ap;
}

uint64_t get_timeofday_timestamp(void)
{
	struct timeval tv;
	uint64_t ts_ofday;
	do_gettimeofday(&tv);
	ts_ofday = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
	return ts_ofday;
}

/*! sensor support type map */
static const struct bmi160_type_mapping_type sensor_type_map[] = {

	{SENSOR_CHIP_ID_BMI, SENSOR_CHIP_REV_ID_BMI, "BMI160/162AB"},
	{SENSOR_CHIP_ID_BMI_C2, SENSOR_CHIP_REV_ID_BMI, "BMI160C2"},
	{SENSOR_CHIP_ID_BMI_C3, SENSOR_CHIP_REV_ID_BMI, "BMI160C3"},
};

/*!bmi160 sensor time depends on ODR */
static const struct bmi_sensor_time_odr_tbl
		sensortime_duration_tbl[TS_MAX_HZ] = {
	{0x010000, 2560000, 0x00ffff},/*2560ms, 0.39hz, odr=resver*/
	{0x008000, 1280000, 0x007fff},/*1280ms, 0.78hz, odr_acc=1*/
	{0x004000, 640000, 0x003fff},/*640ms, 1.56hz, odr_acc=2*/
	{0x002000, 320000, 0x001fff},/*320ms, 3.125hz, odr_acc=3*/
	{0x001000, 160000, 0x000fff},/*160ms, 6.25hz, odr_acc=4*/
	{0x000800, 80000,  0x0007ff},/*80ms, 12.5hz*/
	{0x000400, 40000, 0x0003ff},/*40ms, 25hz, odr_acc = odr_gyro =6*/
	{0x000200, 20000, 0x0001ff},/*20ms, 50hz, odr = 7*/
	{0x000100, 10000, 0x0000ff},/*10ms, 100hz, odr=8*/
	{0x000080, 5000, 0x00007f},/*5ms, 200hz, odr=9*/
	{0x000040, 2500, 0x00003f},/*2.5ms, 400hz, odr=10*/
	{0x000020, 1250, 0x00001f},/*1.25ms, 800hz, odr=11*/
	{0x000010, 625, 0x00000f},/*0.625ms, 1600hz, odr=12*/
};

static void bmi_delay(u32 msec)
{
	if (msec <= 20)
		usleep_range(msec * 1000, msec * 1000);
	else
		msleep(msec);
}

static void bmi_dump_reg(struct bmi_client_data *client_data)
{
	#define REG_MAX0 0x24
	#define REG_MAX1 0x56
	int i;
	u8 dbg_buf0[REG_MAX0];
	u8 dbg_buf1[REG_MAX1];
	u8 dbg_buf_str0[REG_MAX0 * 3 + 1] = "";
	u8 dbg_buf_str1[REG_MAX1 * 3 + 1] = "";

	dev_notice(client_data->dev, "\nFrom 0x00:\n");

	client_data->device.bus_read(client_data->device.dev_addr,
			BMI_REG_NAME(USER_CHIP_ID), dbg_buf0, REG_MAX0);
	for (i = 0; i < REG_MAX0; i++) {
		snprintf(dbg_buf_str0 + i * 3, 16, "%02x%c", dbg_buf0[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_notice(client_data->dev, "%s\n", dbg_buf_str0);

	client_data->device.bus_read(client_data->device.dev_addr,
			BMI160_USER_ACCEL_CONFIG_ADDR, dbg_buf1, REG_MAX1);
	dev_notice(client_data->dev, "\nFrom 0x40:\n");
	for (i = 0; i < REG_MAX1; i++) {
		snprintf(dbg_buf_str1 + i * 3, 16, "%02x%c", dbg_buf1[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_notice(client_data->dev, "\n%s\n", dbg_buf_str1);
	}


void bmi_fifo_frame_bytes_extend_calc(
	struct bmi_client_data *client_data,
	unsigned int *fifo_frmbytes_extend)
{

	switch (client_data->fifo_data_sel) {
	case BMI_FIFO_A_SEL:
	case BMI_FIFO_G_SEL:
		*fifo_frmbytes_extend = 7;
		break;
	case BMI_FIFO_G_A_SEL:
		*fifo_frmbytes_extend = 13;
		break;
	case BMI_FIFO_M_SEL:
		*fifo_frmbytes_extend = 9;
		break;
	case BMI_FIFO_M_A_SEL:
	case BMI_FIFO_M_G_SEL:
		/*8(mag) + 6(gyro or acc) +1(head) = 15*/
		*fifo_frmbytes_extend = 15;
		break;
	case BMI_FIFO_M_G_A_SEL:
		/*8(mag) + 6(gyro or acc) + 6 + 1 = 21*/
		*fifo_frmbytes_extend = 21;
		break;
	default:
		*fifo_frmbytes_extend = 0;
		break;

	};

}
static int bmi_check_chip_id(struct bmi_client_data *client_data)
{
	int8_t err = 0;
	int8_t i = 0;
	uint8_t chip_id = 0;
	u8 bmi_sensor_cnt = sizeof(sensor_type_map)
				/ sizeof(struct bmi160_type_mapping_type);
	/* read and check chip id */
	if (client_data->device.bus_read(client_data->device.dev_addr,
			BMI_REG_NAME(USER_CHIP_ID), &chip_id, 1) < 0) {
		dev_err(client_data->dev,
				"Bosch Sensortec Device not found"
					"read chip_id:%d\n", chip_id);
		err = -ENODEV;
		return err;
	} else {
		for (i = 0; i < bmi_sensor_cnt; i++) {
			if (sensor_type_map[i].chip_id == chip_id) {
				client_data->chip_id = chip_id;
				dev_notice(client_data->dev,
				"Bosch Sensortec Device detected, "
		"HW IC name: %s\n", sensor_type_map[i].sensor_name);
				break;
			}
		}
	}
	return err;

}

static int bmi_pmu_set_suspend(struct bmi_client_data *client_data)
{
	int err = 0;
	if (client_data == NULL)
		return -EINVAL;
	else {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[SENSOR_PM_SUSPEND]);
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[SENSOR_PM_SUSPEND]);
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_mag_arr[SENSOR_PM_SUSPEND]);
		client_data->pw.acc_pm = BMI_ACC_PM_SUSPEND;
		client_data->pw.gyro_pm = BMI_GYRO_PM_SUSPEND;
		client_data->pw.mag_pm = BMI_MAG_PM_SUSPEND;
	}

	return err;
}

static int bmi_get_err_status(struct bmi_client_data *client_data)
{
	int err = 0;

	err = BMI_CALL_API(get_error_status)(&client_data->err_st.fatal_err,
		&client_data->err_st.err_code, &client_data->err_st.i2c_fail,
	&client_data->err_st.drop_cmd, &client_data->err_st.mag_drdy_err);
	return err;
}
static ssize_t bmi160_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);


	return snprintf(buf, 16, "0x%x\n", client_data->chip_id);
}

#ifdef CONFIG_ENABLE_ACC_GYRO_BUFFERING
static int bmi_gyro_read_bootsampl(struct bmi_client_data *client_data,
		unsigned long enable_read)
{
	int i = 0;

	if (enable_read) {
		client_data->gyro_buffer_bmi160_samples = false;
		for (i = 0; i < client_data->gyro_bufsample_cnt; i++) {
			LOGDI(client_data->dev,
				"gyro_cnt=%d,x=%d,y=%d,z=%d,tsec=%d,nsec=%lld\n",
				i, client_data->bmi160_gyro_samplist[i]->xyz[0],
				client_data->bmi160_gyro_samplist[i]->xyz[1],
				client_data->bmi160_gyro_samplist[i]->xyz[2],
				client_data->bmi160_gyro_samplist[i]->tsec,
				client_data->bmi160_gyro_samplist[i]->tnsec)
			input_report_abs(client_data->gyrobuf_dev, ABS_X,
				client_data->bmi160_gyro_samplist[i]->xyz[0]);
			input_report_abs(client_data->gyrobuf_dev, ABS_Y,
				client_data->bmi160_gyro_samplist[i]->xyz[1]);
			input_report_abs(client_data->gyrobuf_dev, ABS_Z,
				client_data->bmi160_gyro_samplist[i]->xyz[2]);
			input_report_abs(client_data->gyrobuf_dev, ABS_RX,
				client_data->bmi160_gyro_samplist[i]->tsec);
			input_report_abs(client_data->gyrobuf_dev, ABS_RY,
				client_data->bmi160_gyro_samplist[i]->tnsec);
			input_sync(client_data->gyrobuf_dev);
		}
	} else {
		/* clean up */
		if (0 != client_data->gyro_bufsample_cnt) {
			for (i = 0; i < BMI_GYRO_MAXSAMPLE; i++)
				kmem_cache_free(client_data->bmi_gyro_cachepool,
					client_data->bmi160_gyro_samplist[i]);
			kmem_cache_destroy(client_data->bmi_gyro_cachepool);
			client_data->gyro_bufsample_cnt = 0;
		}

	}
	/*SYN_CONFIG indicates end of data*/
	input_event(client_data->gyrobuf_dev, EV_SYN, SYN_CONFIG, 0xFFFFFFFF);
	input_sync(client_data->gyrobuf_dev);
	LOGDI(client_data->dev,	"End of gyro samples bufsample_cnt=%d\n",
			client_data->gyro_bufsample_cnt)
	return 0;
}
static int bmi_acc_read_bootsampl(struct bmi_client_data *client_data,
		unsigned long enable_read)
{
	int i = 0;

	if (enable_read) {
		client_data->acc_buffer_bmi160_samples = false;
		for (i = 0; i < client_data->acc_bufsample_cnt; i++) {
			LOGDI(client_data->dev,
				"acc_cnt=%d,x=%d,y=%d,z=%d,tsec=%d,nsec=%lld\n",
				i, client_data->bmi160_acc_samplist[i]->xyz[0],
				client_data->bmi160_acc_samplist[i]->xyz[1],
				client_data->bmi160_acc_samplist[i]->xyz[2],
				client_data->bmi160_acc_samplist[i]->tsec,
				client_data->bmi160_acc_samplist[i]->tnsec)
			input_report_abs(client_data->accbuf_dev, ABS_X,
				client_data->bmi160_acc_samplist[i]->xyz[0]);
			input_report_abs(client_data->accbuf_dev, ABS_Y,
				client_data->bmi160_acc_samplist[i]->xyz[1]);
			input_report_abs(client_data->accbuf_dev, ABS_Z,
				client_data->bmi160_acc_samplist[i]->xyz[2]);
			input_report_abs(client_data->accbuf_dev, ABS_RX,
				client_data->bmi160_acc_samplist[i]->tsec);
			input_report_abs(client_data->accbuf_dev, ABS_RY,
				client_data->bmi160_acc_samplist[i]->tnsec);
			input_sync(client_data->accbuf_dev);
		}
	} else {
		/* clean up */
		if (0 != client_data->acc_bufsample_cnt) {
			for (i = 0; i < BMI_ACC_MAXSAMPLE; i++)
				kmem_cache_free(client_data->bmi_acc_cachepool,
					client_data->bmi160_acc_samplist[i]);
			kmem_cache_destroy(client_data->bmi_acc_cachepool);
			client_data->acc_bufsample_cnt = 0;
		}

	}
	/*SYN_CONFIG indicates end of data*/
	input_event(client_data->accbuf_dev, EV_SYN, SYN_CONFIG, 0xFFFFFFFF);
	input_sync(client_data->accbuf_dev);
	LOGDI(client_data->dev,
			"End of acc samples bufsample_cnt=%d\n",
			client_data->acc_bufsample_cnt)
	return 0;
}
static ssize_t read_gyro_boot_sample_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			client_data->read_gyro_boot_sample);
}

static ssize_t read_gyro_boot_sample_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	unsigned long enable = 0;

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable > 1)	{
		err = dev_err(client_data->dev,
				"Invalid value of input, input=%ld\n", enable);
		return -EINVAL;
	}
	err = bmi_gyro_read_bootsampl(client_data, enable);
	if (err)
		return err;
	client_data->read_gyro_boot_sample = enable;
	return count;

}

static ssize_t read_acc_boot_sample_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			client_data->read_acc_boot_sample);
}

static ssize_t read_acc_boot_sample_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	unsigned long enable = 0;

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable > 1)	{
		err = dev_err(client_data->dev,
				"Invalid value of input, input=%ld\n", enable);
		return -EINVAL;
	}
	err = bmi_acc_read_bootsampl(client_data, enable);
	if (err)
		return err;
	client_data->read_acc_boot_sample = enable;
	return count;
}
#endif

static ssize_t bmi160_err_st_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	int err = 0;
	err = bmi_get_err_status(client_data);
	if (err)
		return err;
	else {
		return snprintf(buf, 128, "fatal_err:0x%x, err_code:%d,\n\n"
		"i2c_fail_err:%d, drop_cmd_err:%d, mag_drdy_err:%d\n",
		client_data->err_st.fatal_err, client_data->err_st.err_code,
		client_data->err_st.i2c_fail, client_data->err_st.drop_cmd,
		client_data->err_st.mag_drdy_err);

	}
}

static ssize_t bmi160_sensor_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	u32 sensor_time;
	err = BMI_CALL_API(get_sensor_time)(&sensor_time);
	if (err)
		return err;
	else
		return snprintf(buf, 16, "0x%x\n", (unsigned int)sensor_time);
}

static ssize_t bmi160_fifo_flush_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long enable;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable)
		err = BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);

	if (err)
		dev_err(client_data->dev, "fifo flush failed!\n");

	return count;

}


static ssize_t bmi160_fifo_bytecount_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned int fifo_bytecount = 0;

	BMI_CALL_API(fifo_length)(&fifo_bytecount);
	err = snprintf(buf, 16, "%u\n", fifo_bytecount);
	return err;
}

static ssize_t bmi160_fifo_bytecount_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	int err;
	unsigned long data;
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	client_data->fifo_bytecount = (unsigned int) data;

	return count;
}

int bmi160_fifo_data_sel_get(struct bmi_client_data *client_data)
{
	int err = 0;
	unsigned char fifo_acc_en, fifo_gyro_en, fifo_mag_en;
	unsigned char fifo_datasel;

	err += BMI_CALL_API(get_fifo_accel_enable)(&fifo_acc_en);
	err += BMI_CALL_API(get_fifo_gyro_enable)(&fifo_gyro_en);
	err += BMI_CALL_API(get_fifo_mag_enable)(&fifo_mag_en);

	if (err)
		return err;

	fifo_datasel = (fifo_acc_en << BMI_ACC_SENSOR) |
			(fifo_gyro_en << BMI_GYRO_SENSOR) |
				(fifo_mag_en << BMI_MAG_SENSOR);

	client_data->fifo_data_sel = fifo_datasel;

	return err;
}
static ssize_t bmi160_fifo_data_sel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	err = bmi160_fifo_data_sel_get(client_data);
	if (err) {
		dev_err(client_data->dev, "get fifo_sel failed!\n");
		return -EINVAL;
	}
	return snprintf(buf, 16, "%d\n", client_data->fifo_data_sel);
}

/* write any value to clear all the fifo data. */
static ssize_t bmi160_fifo_data_sel_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	int err;
	unsigned long data;
	unsigned char fifo_datasel;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* data format: aimed 0b0000 0x(m)x(g)x(a), x:1 enable, 0:disable*/
	if (data > 7)
		return -EINVAL;

	fifo_datasel = (unsigned char)data;
	err += BMI_CALL_API(set_fifo_accel_enable)
			((fifo_datasel & (1 << BMI_ACC_SENSOR)) ? 1 :  0);
	err += BMI_CALL_API(set_fifo_gyro_enable)
			(fifo_datasel & (1 << BMI_GYRO_SENSOR) ? 1 : 0);
	err += BMI_CALL_API(set_fifo_mag_enable)
			((fifo_datasel & (1 << BMI_MAG_SENSOR)) ? 1 : 0);
	err += BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);

	if (err)
		return -EIO;
	else {
		dev_notice(client_data->dev, "FIFO A_en:%d, G_en:%d, M_en:%d\n",
			(fifo_datasel & (1 << BMI_ACC_SENSOR)) ? 1 :  0,
			(fifo_datasel & (1 << BMI_GYRO_SENSOR) ? 1 : 0),
			((fifo_datasel & (1 << BMI_MAG_SENSOR)) ? 1 : 0));
		client_data->fifo_time2 = 0;
		client_data->fifo_data_sel = fifo_datasel;
	}

	return count;
}


static int bmi160_report_accel_data(struct iio_dev *indio_dev,
	int header, struct bmi160_accel_t acc, u64 t)
{
	u8 buf_16[IIO_AORGBUFFER] = {0};
	memcpy(buf_16, &acc, sizeof(acc));
	memcpy(buf_16+8, &t, sizeof(t));
#ifdef BMI160_DEBUG
	if (debug_level & 0x01)
		printk(KERN_INFO "data %s %d %d %d %llu\n",
			ACC_FIFO_HEAD, acc.x, acc.y, acc.z,
			t);
#endif

	store_acc_boot_sample(acc.x, acc.y, acc.z);
	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf_16);
	mutex_unlock(&indio_dev->mlock);

	return 0;
}

static int bmi160_report_gyro_data(struct iio_dev *indio_dev,
	int header, struct bmi160_gyro_t gyro, u64 t)
{
	u8 buf_16[IIO_AORGBUFFER] = {0};
	memcpy(buf_16, &gyro, sizeof(gyro));
	memcpy(buf_16+8, &t, sizeof(t));
#ifdef BMI160_DEBUG
	if (debug_level & 0x02)
		printk(KERN_INFO "data %s %d %d %d %llu\n",
			GYRO_FIFO_HEAD, gyro.x, gyro.y, gyro.z,
			t);
#endif

	store_gyro_boot_sample(gyro.x, gyro.y, gyro.z);
	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf_16);
	mutex_unlock(&indio_dev->mlock);

	return 0;
}

static int bmi160_report_mag_data(struct iio_dev *indio_dev,
	int header, struct bmi160_mag_xyz_s32_t mag, u64 t)
{
	u8 buf_16[IIO_AORGBUFFER] = {0};
	struct iio_buffer *ring = indio_dev->buffer;
	memcpy(buf_16, &mag, sizeof(mag));
	memcpy(buf_16+8, &t, sizeof(t));
#ifdef BMI160_DEBUG
	if (debug_level & 0x04)
		printk(KERN_INFO "data %s %d %d %d %llu\n",
			MAG_FIFO_HEAD, mag.x, mag.y, mag.z,
			t);
#endif
	/*iio_push_to_buffers(indio_dev, buf_16);*/
	ring->access->store_to(indio_dev->buffer, buf_16);
	return 0;
}

struct bmi160_accel_t acc_frame_arr[FIFO_FRAME_CNT];
struct bmi160_gyro_t gyro_frame_arr[FIFO_FRAME_CNT];
struct bmi160_mag_xyzr_t mag_frame_arr[FIFO_FRAME_CNT];

int bmi_fifo_analysis_handle(struct bmi_client_data *client_data,
		u8 *fifo_data, u16 fifo_length)
{
	u8 frame_head = 0;/* every frame head*/
	/*u8 skip_frame_cnt = 0;*/
	u8 acc_frm_cnt = 0;/*0~146*/
	u8 gyro_frm_cnt = 0;
	u8 mag_frm_cnt = 0;
	/*u8 tmp_odr = 0;*/
	/*uint64_t current_apts_us = 0;*/
	/*fifo data last frame start_index A G M*/
	u64 current_frm_ts;
	u16 fifo_index = 0;/* fifo data buff index*/
	u16 fifo_index_tmp = 0;
	u16 i = 0;
	s8 last_return_st = 0;
	int err = 0;
	unsigned int frame_bytes = 0;
	struct bmi160_mag_xyzr_t mag;
	struct bmi160_gyro_t gyro;
	struct bmi160_accel_t acc;
	struct bmi160_mag_xyz_s32_t mag_comp_xyz;

	int hdr = 0;
	struct odr_t odr;
	memset(&odr, 0, sizeof(odr));
	memset(&acc, 0, sizeof(acc));
	memset(&gyro, 0, sizeof(gyro));
	memset(&mag, 0, sizeof(mag));
	memset(&mag_comp_xyz, 0, sizeof(mag_comp_xyz));
	for (i = 0; i < FIFO_FRAME_CNT; i++) {
		memset(&mag_frame_arr[i], 0, sizeof(struct bmi160_mag_xyzr_t));
		memset(&acc_frame_arr[i], 0, sizeof(struct bmi160_accel_t));
		memset(&gyro_frame_arr[i], 0, sizeof(struct bmi160_gyro_t));
	}
	/*current_apts_us = get_current_timestamp();*/
	/* no fifo select for bmi sensor*/
	if (!client_data->fifo_data_sel) {
		dev_err(client_data->dev,
				"No select any sensor FIFO for BMI16x\n");
		return -EINVAL;
	}
	/*driver need read acc_odr/gyro_odr/mag_odr*/
	if ((client_data->fifo_data_sel) & (1 << BMI_ACC_SENSOR))
		odr.acc_odr = client_data->odr.acc_odr;
	if ((client_data->fifo_data_sel) & (1 << BMI_GYRO_SENSOR))
		odr.gyro_odr = client_data->odr.gyro_odr;
	if ((client_data->fifo_data_sel) & (1 << BMI_MAG_SENSOR))
		odr.mag_odr = client_data->odr.mag_odr;
	bmi_fifo_frame_bytes_extend_calc(client_data, &frame_bytes);
	for (fifo_index = 0; fifo_index < fifo_length;) {
		/* conside limited HW i2c burst reading issue,
		need to re-calc index 256 512 768 1024...*/
		if ((fifo_index_tmp >> 8) != (fifo_index >> 8)) {
			if (fifo_data[fifo_index_tmp] ==
				fifo_data[(fifo_index >> 8)<<8]) {
				fifo_index = (fifo_index >> 8) << 8;
				fifo_length +=
					(fifo_index - fifo_index_tmp + 1);
			}
		}
		fifo_index_tmp = fifo_index;
		/* compare index with 256/512/ before doing parsing*/
		if (((fifo_index + frame_bytes) >> 8) != (fifo_index >> 8)) {
			fifo_index = ((fifo_index + frame_bytes) >> 8) << 8;
			continue;
		}

		frame_head = fifo_data[fifo_index];

		switch (frame_head) {
			/*skip frame 0x40 22 0x84*/
		case FIFO_HEAD_SKIP_FRAME:
		/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + 1 > fifo_length) {
				last_return_st = FIFO_SKIP_OVER_LEN;
				break;
			}
			/*skip_frame_cnt = fifo_data[fifo_index];*/
			fifo_index = fifo_index + 1;
		break;

			/*M & G & A*/
		case FIFO_HEAD_M_G_A:
		{/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + MGA_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_M_G_A_OVER_LEN;
				break;
			}

			/* mag frm index = gyro */
			mag_frm_cnt = gyro_frm_cnt;
			mag_frame_arr[mag_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			mag_frame_arr[mag_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			mag_frame_arr[mag_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			mag_frame_arr[mag_frm_cnt].r =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];

			gyro_frame_arr[gyro_frm_cnt].x =
				fifo_data[fifo_index + 9] << 8 |
					fifo_data[fifo_index + 8];
			gyro_frame_arr[gyro_frm_cnt].y =
				fifo_data[fifo_index + 11] << 8 |
					fifo_data[fifo_index + 10];
			gyro_frame_arr[gyro_frm_cnt].z =
				fifo_data[fifo_index + 13] << 8 |
					fifo_data[fifo_index + 12];

			acc_frame_arr[acc_frm_cnt].x =
				fifo_data[fifo_index + 15] << 8 |
					fifo_data[fifo_index + 14];
			acc_frame_arr[acc_frm_cnt].y =
				fifo_data[fifo_index + 17] << 8 |
					fifo_data[fifo_index + 16];
			acc_frame_arr[acc_frm_cnt].z =
				fifo_data[fifo_index + 19] << 8 |
					fifo_data[fifo_index + 18];

			mag_frm_cnt++;/* M fram_cnt++ */
			gyro_frm_cnt++;/* G fram_cnt++ */
			acc_frm_cnt++;/* A fram_cnt++ */

			fifo_index = fifo_index + MGA_BYTES_FRM;
			break;
		}

		case FIFO_HEAD_M_A:
		{/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + MA_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_M_A_OVER_LEN;
				break;
			}

			mag_frm_cnt = acc_frm_cnt;

			mag_frame_arr[mag_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			mag_frame_arr[mag_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			mag_frame_arr[mag_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			mag_frame_arr[mag_frm_cnt].r =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];

			acc_frame_arr[acc_frm_cnt].x =
				fifo_data[fifo_index + 9] << 8 |
					fifo_data[fifo_index + 8];
			acc_frame_arr[acc_frm_cnt].y =
				fifo_data[fifo_index + 11] << 8 |
					fifo_data[fifo_index + 10];
			acc_frame_arr[acc_frm_cnt].z =
				fifo_data[fifo_index + 13] << 8 |
					fifo_data[fifo_index + 12];

			mag_frm_cnt++;/* M fram_cnt++ */
			acc_frm_cnt++;/* A fram_cnt++ */

			fifo_index = fifo_index + MA_BYTES_FRM;
			break;
		}

		case FIFO_HEAD_M_G:
		{/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + MG_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_M_G_OVER_LEN;
				break;
			}

			mag_frm_cnt = gyro_frm_cnt;
			mag_frame_arr[mag_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			mag_frame_arr[mag_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			mag_frame_arr[mag_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			mag_frame_arr[mag_frm_cnt].r =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];

			gyro_frame_arr[gyro_frm_cnt].x =
				fifo_data[fifo_index + 9] << 8 |
					fifo_data[fifo_index + 8];
			gyro_frame_arr[gyro_frm_cnt].y =
				fifo_data[fifo_index + 11] << 8 |
					fifo_data[fifo_index + 10];
			gyro_frame_arr[gyro_frm_cnt].z =
				fifo_data[fifo_index + 13] << 8 |
					fifo_data[fifo_index + 12];

			mag_frm_cnt++;/* M fram_cnt++ */
			gyro_frm_cnt++;/* G fram_cnt++ */
			fifo_index = fifo_index + MG_BYTES_FRM;
		break;
		}

		case FIFO_HEAD_G_A:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + GA_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_G_A_OVER_LEN;
				break;
			}
			gyro_frame_arr[gyro_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			gyro_frame_arr[gyro_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			gyro_frame_arr[gyro_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];

			acc_frame_arr[acc_frm_cnt].x =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];
			acc_frame_arr[acc_frm_cnt].y =
				fifo_data[fifo_index + 9] << 8 |
					fifo_data[fifo_index + 8];
			acc_frame_arr[acc_frm_cnt].z =
				fifo_data[fifo_index + 11] << 8 |
					fifo_data[fifo_index + 10];
			gyro_frm_cnt++;
			acc_frm_cnt++;
			fifo_index = fifo_index + GA_BYTES_FRM;

			break;
		}
		case FIFO_HEAD_A:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + A_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_A_OVER_LEN;
				break;
			}

			acc_frame_arr[acc_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			acc_frame_arr[acc_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			acc_frame_arr[acc_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			acc_frm_cnt++;/*acc_frm_cnt*/
			fifo_index = fifo_index + A_BYTES_FRM;
			break;
		}
		case FIFO_HEAD_G:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + G_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_G_OVER_LEN;
				break;
			}

			gyro_frame_arr[gyro_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			gyro_frame_arr[gyro_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			gyro_frame_arr[gyro_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			gyro_frm_cnt++;/*gyro_frm_cnt*/

			fifo_index = fifo_index + G_BYTES_FRM;
			break;
		}
		case FIFO_HEAD_M:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + M_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_M_OVER_LEN;
				break;
			}

			mag_frame_arr[mag_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			mag_frame_arr[mag_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			mag_frame_arr[mag_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			mag_frame_arr[mag_frm_cnt].r =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];

			mag_frm_cnt++;/* M fram_cnt++ */

			fifo_index = fifo_index + M_BYTES_FRM;
			break;
		}

		/* sensor time frame*/
		case FIFO_HEAD_SENSOR_TIME:
		{
			/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 3 > fifo_length) {
				last_return_st = FIFO_SENSORTIME_RETURN;
				break;
			}
			/*fifo sensor time frame index + 3*/
			fifo_index = fifo_index + 3;
			break;
		}
		case FIFO_HEAD_OVER_READ_LSB:
			/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 1 > fifo_length) {
				last_return_st = FIFO_OVER_READ_RETURN;
				break;
			}
			if (fifo_data[fifo_index] ==
					FIFO_HEAD_OVER_READ_MSB) {
				/*fifo over read frame index + 1*/
				fifo_index = fifo_index + 1;
				break;
			} else {
				last_return_st = FIFO_OVER_READ_RETURN;
				break;
			}
		break;
		default:
			last_return_st = 1;
		break;

		}
			if (last_return_st)
				break;

	}
/*Acc Only*/
	if (client_data->fifo_data_sel == BMI_FIFO_A_SEL) {
		for (i = 0; i < acc_frm_cnt; i++) {
			if (client_data->fifo_time2 == 0) {
				current_frm_ts = client_data->fifo_time -
		client_data->del_time - (acc_frm_cnt-i) *
		sensortime_duration_tbl[odr.acc_odr].ts_duration_us;
		if (i == (acc_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		} else {
			current_frm_ts = client_data->fifo_time -
		client_data->del_time -
		(div64_u64(client_data->fifo_time - client_data->fifo_time2,
		acc_frm_cnt)) * (acc_frm_cnt-i);
		if (i == (acc_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		}
		hdr = FIFO_HEAD_A;
		acc = acc_frame_arr[i];
		bmi160_report_accel_data(accl_iio_private, hdr,
			acc, current_frm_ts);
		}
	}
	/*only for G*/
	if (client_data->fifo_data_sel == BMI_FIFO_G_SEL) {
		for (i = 0; i < gyro_frm_cnt; i++) {
			if (client_data->fifo_time2 == 0) {
				current_frm_ts = client_data->fifo_time -
		client_data->del_time - (gyro_frm_cnt-i) *
		sensortime_duration_tbl[odr.gyro_odr].ts_duration_us;
		if (i == (gyro_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		} else {
			current_frm_ts = client_data->fifo_time -
		client_data->del_time -
		(div64_u64(client_data->fifo_time - client_data->fifo_time2,
		gyro_frm_cnt)) * (gyro_frm_cnt-i);
		if (i == (gyro_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		}
		hdr = FIFO_HEAD_G;
		gyro = gyro_frame_arr[i];
		bmi160_report_gyro_data(gyro_iio_private, hdr,
			gyro, current_frm_ts);
		}
	}
	/*only for M*/
	if (client_data->fifo_data_sel == BMI_FIFO_M_SEL) {
		for (i = 0; i < mag_frm_cnt; i++) {
			/*current_frm_ts += 256;*/
			if (mag_frame_arr[i].x) {
				if (client_data->fifo_time2 == 0) {
					current_frm_ts =
		client_data->fifo_time -
		client_data->del_time - (mag_frm_cnt-i) *
		sensortime_duration_tbl[odr.mag_odr].ts_duration_us;
		if (i == (mag_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		} else {
			current_frm_ts = client_data->fifo_time -
		client_data->del_time -
		(div64_u64(client_data->fifo_time - client_data->fifo_time2,
		mag_frm_cnt)) * (mag_frm_cnt-i);
		if (i == (mag_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		}
#if defined(BMI160_AKM09912_SUPPORT)
			mag_comp_xyz.x = mag_frame_arr[i].x;
			mag_comp_xyz.y = mag_frame_arr[i].y;
			mag_comp_xyz.z = mag_frame_arr[i].z;
			bmi160_bst_akm09912_compensate_xyz_raw(&mag_comp_xyz);
#else
			mag.x = mag_frame_arr[i].x >> 3;
			mag.y = mag_frame_arr[i].y >> 3;
			mag.z = mag_frame_arr[i].z >> 1;
			mag.r = mag_frame_arr[i].r >> 2;
			bmi160_bmm150_mag_compensate_xyz_raw(
			&mag_comp_xyz, mag);
#endif
			hdr = FIFO_HEAD_M;
			bmi160_report_mag_data(magn_iio_private, hdr,
				mag_comp_xyz, current_frm_ts);
			}
		}
	}

/*only for A G && A M G*/
if ((client_data->fifo_data_sel == BMI_FIFO_G_A_SEL) ||
		(client_data->fifo_data_sel == BMI_FIFO_M_G_A_SEL)) {

	for (i = 0; i < gyro_frm_cnt; i++) {
		if (client_data->fifo_time2 == 0) {
			current_frm_ts = client_data->fifo_time -
		client_data->del_time - (gyro_frm_cnt-i) *
		sensortime_duration_tbl[odr.gyro_odr].ts_duration_us;
		if (i == (gyro_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		} else {
			current_frm_ts = client_data->fifo_time -
		client_data->del_time -
		(div64_u64(client_data->fifo_time - client_data->fifo_time2,
		gyro_frm_cnt)) * (gyro_frm_cnt-i);
		if (i == (gyro_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		}
		if (mag_frame_arr[i].x) {
#if defined(BMI160_AKM09912_SUPPORT)
			mag_comp_xyz.x = mag_frame_arr[i].x;
			mag_comp_xyz.y = mag_frame_arr[i].y;
			mag_comp_xyz.z = mag_frame_arr[i].z;
			bmi160_bst_akm09912_compensate_xyz_raw(&mag_comp_xyz);
#else
			mag.x = mag_frame_arr[i].x >> 3;
			mag.y = mag_frame_arr[i].y >> 3;
			mag.z = mag_frame_arr[i].z >> 1;
			mag.r = mag_frame_arr[i].r >> 2;
			bmi160_bmm150_mag_compensate_xyz_raw(
			&mag_comp_xyz, mag);
#endif
			hdr = FIFO_HEAD_G;
			gyro = gyro_frame_arr[i];
			bmi160_report_gyro_data(gyro_iio_private, hdr,
				gyro, current_frm_ts);
			hdr = FIFO_HEAD_A;
			if (acc_frame_arr[i].x != 0) {
				acc = acc_frame_arr[i];
				bmi160_report_accel_data(accl_iio_private, hdr,
				acc, current_frm_ts);
			}
			hdr = FIFO_HEAD_M;
			bmi160_report_mag_data(magn_iio_private, hdr,
				mag_comp_xyz, current_frm_ts);
		} else {
			hdr = FIFO_HEAD_G;
			gyro = gyro_frame_arr[i];
			bmi160_report_gyro_data(gyro_iio_private, hdr,
				gyro, current_frm_ts);
			hdr = FIFO_HEAD_A;
			if (acc_frame_arr[i].x != 0) {
				acc = acc_frame_arr[i];
				bmi160_report_accel_data(accl_iio_private, hdr,
				acc, current_frm_ts);
			}
		}
	}

}

/*only for A M */
if (client_data->fifo_data_sel == BMI_FIFO_M_A_SEL) {
	for (i = 0; i < acc_frm_cnt; i++) {
		if (client_data->fifo_time2 == 0) {
			current_frm_ts = client_data->fifo_time -
		client_data->del_time - (acc_frm_cnt-i) *
		sensortime_duration_tbl[odr.acc_odr].ts_duration_us;
		if (i == (acc_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		} else {
			current_frm_ts = client_data->fifo_time -
		client_data->del_time -
		(div64_u64(client_data->fifo_time - client_data->fifo_time2,
		acc_frm_cnt)) * (acc_frm_cnt-i);
		if (i == (acc_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		}
		if (mag_frame_arr[i].x) {
#if defined(BMI160_AKM09912_SUPPORT)
			mag_comp_xyz.x = mag_frame_arr[i].x;
			mag_comp_xyz.y = mag_frame_arr[i].y;
			mag_comp_xyz.z = mag_frame_arr[i].z;
			bmi160_bst_akm09912_compensate_xyz_raw(&mag_comp_xyz);
#else
			mag.x = mag_frame_arr[i].x >> 3;
			mag.y = mag_frame_arr[i].y >> 3;
			mag.z = mag_frame_arr[i].z >> 1;
			mag.r = mag_frame_arr[i].r >> 2;
			bmi160_bmm150_mag_compensate_xyz_raw(
			&mag_comp_xyz, mag);
#endif
			hdr = FIFO_HEAD_A;
			acc = acc_frame_arr[i];
			bmi160_report_accel_data(accl_iio_private, hdr,
				acc, current_frm_ts);
			hdr = FIFO_HEAD_M;
			bmi160_report_mag_data(magn_iio_private, hdr,
				mag_comp_xyz, current_frm_ts);
		} else {
			hdr = FIFO_HEAD_A;
			acc = acc_frame_arr[i];
			bmi160_report_accel_data(accl_iio_private, hdr,
				acc, current_frm_ts);
		}
	}
}

/*only forG M*/
if (client_data->fifo_data_sel == BMI_FIFO_M_G_SEL) {
	for (i = 0; i < gyro_frm_cnt; i++) {
		if (client_data->fifo_time2 == 0) {
			current_frm_ts = client_data->fifo_time -
		client_data->del_time - (gyro_frm_cnt-i) *
		sensortime_duration_tbl[odr.gyro_odr].ts_duration_us;
		if (i == (gyro_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		} else {
			current_frm_ts = client_data->fifo_time -
		client_data->del_time -
		(div64_u64(client_data->fifo_time - client_data->fifo_time2,
		gyro_frm_cnt)) * (gyro_frm_cnt-i);
		if (i == (gyro_frm_cnt - 1))
			client_data->fifo_time2 = client_data->fifo_time;
		}
		if (mag_frame_arr[i].x) {
#if defined(BMI160_AKM09912_SUPPORT)
			mag_comp_xyz.x = mag_frame_arr[i].x;
			mag_comp_xyz.y = mag_frame_arr[i].y;
			mag_comp_xyz.z = mag_frame_arr[i].z;
			bmi160_bst_akm09912_compensate_xyz_raw(&mag_comp_xyz);
#else
			mag.x = mag_frame_arr[i].x >> 3;
			mag.y = mag_frame_arr[i].y >> 3;
			mag.z = mag_frame_arr[i].z >> 1;
			mag.r = mag_frame_arr[i].r >> 2;
			bmi160_bmm150_mag_compensate_xyz_raw(
			&mag_comp_xyz, mag);
#endif
			hdr = FIFO_HEAD_G;
			gyro = gyro_frame_arr[i];
			bmi160_report_gyro_data(gyro_iio_private, hdr,
				gyro, current_frm_ts);
			hdr = FIFO_HEAD_M;
			bmi160_report_mag_data(magn_iio_private, hdr,
				mag_comp_xyz, current_frm_ts);
		} else {
			hdr = FIFO_HEAD_G;
			gyro = gyro_frame_arr[i];
			bmi160_report_gyro_data(gyro_iio_private, hdr,
				gyro, current_frm_ts);
		}
	}
}

	return err;
}

static ssize_t bmi160_fifo_watermark_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0xff;

	err = BMI_CALL_API(get_fifo_wm)(&data);

	if (err)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_fifo_watermark_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long data;
	unsigned char fifo_watermark;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	fifo_watermark = (unsigned char)data;
	err = BMI_CALL_API(set_fifo_wm)(fifo_watermark);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_fifo_header_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0xff;

	err = BMI_CALL_API(get_fifo_header_enable)(&data);

	if (err)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_fifo_header_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	int err;
	unsigned long data;
	unsigned char fifo_header_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	if (data > 1)
		return -ENOENT;

	fifo_header_en = (unsigned char)data;
	err = BMI_CALL_API(set_fifo_header_enable)(fifo_header_en);
	if (err)
		return -EIO;

	client_data->fifo_head_en = fifo_header_en;

	return count;
}

static ssize_t bmi160_fifo_time_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0;

	err = BMI_CALL_API(get_fifo_time_enable)(&data);

	if (!err)
		err = snprintf(buf, 16, "%d\n", data);

	return err;
}

static ssize_t bmi160_fifo_time_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long data;
	unsigned char fifo_ts_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	fifo_ts_en = (unsigned char)data;

	err = BMI_CALL_API(set_fifo_time_enable)(fifo_ts_en);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_fifo_int_tag_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char fifo_tag_int1 = 0;
	unsigned char fifo_tag_int2 = 0;
	unsigned char fifo_tag_int;

	err += BMI_CALL_API(get_fifo_tag_intr1_enable)(&fifo_tag_int1);
	err += BMI_CALL_API(get_fifo_tag_intr2_enable)(&fifo_tag_int2);

	fifo_tag_int = (fifo_tag_int1 << BMI160_INT0) |
			(fifo_tag_int2 << BMI160_INT1);

	if (!err)
		err = snprintf(buf, 16, "%d\n", fifo_tag_int);

	return err;
}

static ssize_t bmi160_fifo_int_tag_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	int err;
	unsigned long data;
	unsigned char fifo_tag_int_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	if (data > 3)
		return -EINVAL;

	fifo_tag_int_en = (unsigned char)data;

	err += BMI_CALL_API(set_fifo_tag_intr1_enable)
			((fifo_tag_int_en & (1 << BMI160_INT0)) ? 1 :  0);
	err += BMI_CALL_API(set_fifo_tag_intr2_enable)
			((fifo_tag_int_en & (1 << BMI160_INT1)) ? 1 :  0);

	if (err) {
		dev_err(client_data->dev, "fifo int tag en err:%d\n", err);
		return -EIO;
	}
	client_data->fifo_int_tag_en = fifo_tag_int_en;

	return count;
}

static int bmi160_set_acc_op_mode(struct bmi_client_data *client_data,
							unsigned long op_mode)
{
	int err = 0;
	unsigned char stc_enable;
	unsigned char std_enable;
	mutex_lock(&client_data->mutex_op_mode);

	if (op_mode < BMI_ACC_PM_MAX) {
		switch (op_mode) {
		case BMI_ACC_PM_NORMAL:
			err = BMI_CALL_API(set_command_register)
			(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_NORMAL]);
			client_data->pw.acc_pm = BMI_ACC_PM_NORMAL;
			bmi_delay(10);
			break;
		case BMI_ACC_PM_LP1:
			err = BMI_CALL_API(set_command_register)
			(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_LP1]);
			client_data->pw.acc_pm = BMI_ACC_PM_LP1;
			bmi_delay(3);
			break;
		case BMI_ACC_PM_SUSPEND:
			BMI_CALL_API(get_step_counter_enable)(&stc_enable);
			BMI_CALL_API(get_step_detector_enable)(&std_enable);
			if ((stc_enable == 0) && (std_enable == 0) &&
				(client_data->sig_flag == 0)) {
				err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_SUSPEND]);
				client_data->pw.acc_pm = BMI_ACC_PM_SUSPEND;
				bmi_delay(10);
			}
			break;
		case BMI_ACC_PM_LP2:
			err = BMI_CALL_API(set_command_register)
			(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_LP2]);
			client_data->pw.acc_pm = BMI_ACC_PM_LP2;
			bmi_delay(3);
			break;
		default:
			mutex_unlock(&client_data->mutex_op_mode);
			return -EINVAL;
		}
	} else {
		mutex_unlock(&client_data->mutex_op_mode);
		return -EINVAL;
	}

	mutex_unlock(&client_data->mutex_op_mode);

	return err;


}
static ssize_t bmi160_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	s16 temp = 0xff;

	err = BMI_CALL_API(get_temp)(&temp);

	if (!err)
		err = snprintf(buf, 16, "0x%x\n", temp);

	return err;
}

#ifdef BMI160_DEBUG
static ssize_t bmi160_debug_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	err = snprintf(buf, 8, "%d\n", debug_level);
	return err;
}
static ssize_t bmi160_debug_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int32_t ret = SUCCESS;
	unsigned long data;
	ret = kstrtoul(buf, 16, &data);
	if (ret)
		return ret;
	debug_level = (uint8_t)data;
	return count;
}
#endif

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
/* accel sensor part */
static ssize_t bmi160_anymot_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data;

	err = BMI_CALL_API(get_intr_any_motion_durn)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_anymot_duration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_intr_any_motion_durn)((unsigned char)data);
	if (err < 0)
		return -EIO;

	return count;
}

static ssize_t bmi160_anymot_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_intr_any_motion_thres)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_anymot_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_intr_any_motion_thres)((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_step_detector_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 data = 0;
	u8 step_det;
	int err;

	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	err = BMI_CALL_API(get_step_detector_enable)(&step_det);
	/*bmi160_get_status0_step_int*/
	if (err < 0)
		return err;
	/*client_data->std will be updated in
	bmi_stepdetector_interrupt_handle */
	if ((step_det == 1) && (client_data->std == 1)) {
		data = 1;
		client_data->std = 0;
		}
	else {
		data = 0;
		}
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_step_detector_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_step_detector_enable)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_step_detector_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_step_detector_enable)((unsigned char)data);
	if (err < 0)
		return -EIO;
	if (data == 0)
		client_data->pedo_data.wkar_step_detector_status = 0;
	return count;
}

static ssize_t bmi160_signification_motion_enable_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;
	/*0x62 (bit 1) INT_MOTION_3 int_sig_mot_sel*/
	err = BMI_CALL_API(get_intr_significant_motion_select)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_signification_motion_enable_store(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/*0x62 (bit 1) INT_MOTION_3 int_sig_mot_sel*/
	err = BMI_CALL_API(set_intr_significant_motion_select)(
		(unsigned char)data);
	if (err < 0)
		return -EIO;
	if (data == 1) {
		err = BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_X_ENABLE, 1);
		err += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Y_ENABLE, 1);
		err += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Z_ENABLE, 1);
		if (err < 0)
			return -EIO;
		client_data->sig_flag = 1;
	} else {
		err = BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_X_ENABLE, 0);
		err += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Y_ENABLE, 0);
		err += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Z_ENABLE, 0);
		if (err < 0)
			return -EIO;
		client_data->sig_flag = 0;
	}
	return count;
}

static ssize_t bmi160_sig_motion_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	int err = 0;
	if (client_data->sig_value == 1) {
		err = snprintf(buf, 16, "%d\n", client_data->sig_value);
		client_data->sig_value = 0;
	} else {
		err = snprintf(buf, 16, "%d\n", client_data->sig_value);
	}
	return err;
}

static int sigmotion_init_interrupts(u8 sig_map_int_pin)
{
	int ret = 0;
/*0x60  */
	ret += bmi160_set_intr_any_motion_thres(0x1e);
/* 0x62(bit 3~2)	0=1.5s */
	ret += bmi160_set_intr_significant_motion_skip(0);
/*0x62(bit 5~4)	1=0.5s*/
	ret += bmi160_set_intr_significant_motion_proof(1);
/*0x50 (bit 0, 1, 2)  INT_EN_0 anymo x y z*/
	ret += bmi160_map_significant_motion_intr(sig_map_int_pin);
/*0x62 (bit 1) INT_MOTION_3	int_sig_mot_sel
close the signification_motion*/
	ret += bmi160_set_intr_significant_motion_select(0);
/*close the anymotion interrupt*/
	ret += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_X_ENABLE, 0);
	ret += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Y_ENABLE, 0);
	ret += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Z_ENABLE, 0);
	if (ret)
		printk(KERN_ERR "bmi160 sig motion failed setting,%d!\n", ret);
	return ret;

}
#endif

static ssize_t bmi160_acc_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char range;
	err = BMI_CALL_API(get_accel_range)(&range);
	if (err)
		return err;
	return snprintf(buf, 16, "%d\n", range);
}

static ssize_t bmi160_acc_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	err = bmi160_check_acc_early_buff_enable_flag(client_data);
	if (err)
		return count;

	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_range)(range);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_acc_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char acc_odr;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	err = BMI_CALL_API(get_accel_output_data_rate)(&acc_odr);
	if (err)
		return err;

	client_data->odr.acc_odr = acc_odr;
	return snprintf(buf, 16, "%d\n", acc_odr);
}

static ssize_t bmi160_acc_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long acc_odr;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	err = bmi160_check_acc_early_buff_enable_flag(client_data);
	if (err)
		return count;

	err = kstrtoul(buf, 10, &acc_odr);
	if (err)
		return err;

	if (acc_odr < 1 || acc_odr > 12)
		return -EIO;

	if (acc_odr < 5)
		err = BMI_CALL_API(set_accel_under_sampling_parameter)(1);
	else
		err = BMI_CALL_API(set_accel_under_sampling_parameter)(0);

	if (err)
		return err;

	err = BMI_CALL_API(set_accel_output_data_rate)(acc_odr);
	if (err)
		return -EIO;
	client_data->odr.acc_odr = acc_odr;
	return count;
}

static ssize_t bmi160_acc_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	int err = 0;
	u8 accel_pmu_status = 0;
	err = BMI_CALL_API(get_accel_power_mode_stat)(
		&accel_pmu_status);

	if (err)
		return err;
	else
	return snprintf(buf, 32, "reg:%d, val:%d\n", accel_pmu_status,
			client_data->pw.acc_pm);
}

static ssize_t bmi160_acc_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	int err = 0;
	unsigned long op_mode;

	err = bmi160_check_acc_early_buff_enable_flag(client_data);
	if (err)
		return count;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	err = bmi160_set_acc_op_mode(client_data, op_mode);
	if (err)
		return err;
	else
		return count;

}

static ssize_t bmi160_acc_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi160_accel_t data;

	int err;

	err = BMI_CALL_API(read_accel_xyz)(&data);
	if (err < 0)
		return err;

	return snprintf(buf, 48, "%hd %hd %hd\n",
			data.x, data.y, data.z);
}

static ssize_t bmi160_acc_fast_calibration_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_accel_x)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_acc_fast_calibration_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	s8 accel_offset_x = 0;
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = BMI_CALL_API(set_accel_foc_trigger)(X_AXIS,
					data, &accel_offset_x);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_acc_fast_calibration_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_accel_y)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_acc_fast_calibration_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	s8 accel_offset_y = 0;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = BMI_CALL_API(set_accel_foc_trigger)(Y_AXIS,
				data, &accel_offset_y);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_acc_fast_calibration_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_accel_z)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_acc_fast_calibration_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	s8 accel_offset_z = 0;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = BMI_CALL_API(set_accel_foc_trigger)(Z_AXIS,
			data, &accel_offset_z);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_acc_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_accel_offset_compensation_xaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}


static ssize_t bmi160_acc_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_offset_compensation_xaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_acc_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_accel_offset_compensation_yaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_acc_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_offset_compensation_yaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_acc_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_accel_offset_compensation_zaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_acc_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_offset_compensation_zaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	u8 raw_data[15] = {0};
	unsigned int sensor_time = 0;

	int err;
	memset(raw_data, 0, sizeof(raw_data));

	err = client_data->device.bus_read(client_data->device.dev_addr,
			BMI160_USER_DATA_8_GYRO_X_LSB__REG, raw_data, 15);
	if (err)
		return err;

	udelay(10);
	sensor_time = (u32)(raw_data[14] << 16 | raw_data[13] << 8
						| raw_data[12]);

	return snprintf(buf, 128, "%d %d %d %d %d %d %u",
					(s16)(raw_data[1] << 8 | raw_data[0]),
				(s16)(raw_data[3] << 8 | raw_data[2]),
				(s16)(raw_data[5] << 8 | raw_data[4]),
				(s16)(raw_data[7] << 8 | raw_data[6]),
				(s16)(raw_data[9] << 8 | raw_data[8]),
				(s16)(raw_data[11] << 8 | raw_data[10]),
				sensor_time);

}

static ssize_t bmi160_step_counter_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);


	err = BMI_CALL_API(get_step_counter_enable)(&data);

	client_data->stc_enable = data;

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_step_counter_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);


	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_step_counter_enable)((unsigned char)data);

	client_data->stc_enable = data;

	if (err < 0)
		return -EIO;
	return count;
}


static ssize_t bmi160_step_counter_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_step_mode)((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_step_counter_clc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = bmi160_clear_step_counter();

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_step_counter_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data;
	int err;
	static u16 last_stc_value;

	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	err = BMI_CALL_API(read_step_count)(&data);

	if (err < 0)
		return err;
	if (data >= last_stc_value) {
		client_data->pedo_data.last_step_counter_value += (
			data - last_stc_value);
		last_stc_value = data;
	} else
		last_stc_value = data;
	return snprintf(buf, 16, "%d\n",
		client_data->pedo_data.last_step_counter_value);
}



static ssize_t bmi160_bmi_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	u8 raw_data[12] = {0};

	int err;
	memset(raw_data, 0, sizeof(raw_data));

	err = client_data->device.bus_read(client_data->device.dev_addr,
			BMI160_USER_DATA_8_GYRO_X_LSB__REG, raw_data, 12);
	if (err)
		return err;
	/*output:gyro x y z acc x y z*/
	return snprintf(buf, 96, "%hd %d %hd %hd %hd %hd\n",
					(s16)(raw_data[1] << 8 | raw_data[0]),
				(s16)(raw_data[3] << 8 | raw_data[2]),
				(s16)(raw_data[5] << 8 | raw_data[4]),
				(s16)(raw_data[7] << 8 | raw_data[6]),
				(s16)(raw_data[9] << 8 | raw_data[8]),
				(s16)(raw_data[11] << 8 | raw_data[10]));

}


static ssize_t bmi160_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);


	return snprintf(buf, 16, "0x%x\n",
				atomic_read(&client_data->selftest_result));
}

/*!
 * @brief store selftest result which make up of acc and gyro
 * format: 0b 0000 xxxx  x:1 failed, 0 success
 * bit3:     gyro_self
 * bit2..0: acc_self z y x
 */
static ssize_t bmi160_selftest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	int err = 0;
	int i = 0;

	u8 acc_selftest = 0;
	u8 gyro_selftest = 0;
	u8 bmi_selftest = 0;
	s16 axis_p_value, axis_n_value;
	u16 diff_axis[3] = {0xff, 0xff, 0xff};
	u8 acc_odr, range, acc_selftest_amp, acc_selftest_sign;

	dev_notice(client_data->dev, "Selftest for BMI16x starting.\n");

	/*soft reset*/
	err = BMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	msleep(70);
	err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_NORMAL]);
	err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_NORMAL]);
	err += BMI_CALL_API(set_accel_under_sampling_parameter)(0);
	err += BMI_CALL_API(set_accel_output_data_rate)(
		BMI160_ACCEL_OUTPUT_DATA_RATE_1600HZ);

	/* set to 8G range*/
	err += BMI_CALL_API(set_accel_range)(BMI160_ACCEL_RANGE_8G);
	/* set to self amp high */
	err += BMI_CALL_API(set_accel_selftest_amp)(BMI_SELFTEST_AMP_HIGH);


	err += BMI_CALL_API(get_accel_output_data_rate)(&acc_odr);
	err += BMI_CALL_API(get_accel_range)(&range);
	err += BMI_CALL_API(get_accel_selftest_amp)(&acc_selftest_amp);
	err += BMI_CALL_API(read_accel_x)(&axis_n_value);

	dev_info(client_data->dev,
			"acc_odr:%d, acc_range:%d, acc_selftest_amp:%d, acc_x:%d\n",
				acc_odr, range, acc_selftest_amp, axis_n_value);

	for (i = X_AXIS; i < AXIS_MAX; i++) {
		axis_n_value = 0;
		axis_p_value = 0;
		/* set every selftest axis */
		/*set_acc_selftest_axis(param),param x:1, y:2, z:3
		* but X_AXIS:0, Y_AXIS:1, Z_AXIS:2
		* so we need to +1*/
		err += BMI_CALL_API(set_accel_selftest_axis)(i + 1);
		msleep(50);
		switch (i) {
		case X_AXIS:
			/* set negative sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(0);
			err += BMI_CALL_API(get_accel_selftest_sign)(
				&acc_selftest_sign);

			msleep(60);
			err += BMI_CALL_API(read_accel_x)(&axis_n_value);
			dev_info(client_data->dev,
			"acc_x_selftest_sign:%d, axis_n_value:%d\n",
			acc_selftest_sign, axis_n_value);

			/* set postive sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(1);
			err += BMI_CALL_API(get_accel_selftest_sign)(
				&acc_selftest_sign);

			msleep(60);
			err += BMI_CALL_API(read_accel_x)(&axis_p_value);
			dev_info(client_data->dev,
			"acc_x_selftest_sign:%d, axis_p_value:%d\n",
			acc_selftest_sign, axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;

		case Y_AXIS:
			/* set negative sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(0);
			msleep(60);
			err += BMI_CALL_API(read_accel_y)(&axis_n_value);
			/* set postive sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(1);
			msleep(60);
			err += BMI_CALL_API(read_accel_y)(&axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;

		case Z_AXIS:
			/* set negative sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(0);
			msleep(60);
			err += BMI_CALL_API(read_accel_z)(&axis_n_value);
			/* set postive sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(1);
			msleep(60);
			err += BMI_CALL_API(read_accel_z)(&axis_p_value);
			/* also start gyro self test */
			err += BMI_CALL_API(set_gyro_selftest_start)(1);
			msleep(60);
			err += BMI_CALL_API(get_gyro_selftest)(&gyro_selftest);

			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;
		default:
			err += -EINVAL;
			break;
		}
		if (err) {
			dev_err(client_data->dev,
				"Failed selftest axis:%s, p_val=%d, n_val=%d\n",
				bmi_axis_name[i], axis_p_value, axis_n_value);
			return -EINVAL;
		}

		/*400mg for acc z axis*/
		if (Z_AXIS == i) {
			if (diff_axis[i] < 1639) {
				acc_selftest |= 1 << i;
				dev_err(client_data->dev,
					"Over selftest minimum for "
					"axis:%s,diff=%d,p_val=%d, n_val=%d\n",
					bmi_axis_name[i], diff_axis[i],
						axis_p_value, axis_n_value);
			}
		} else {
			/*800mg for x or y axis*/
			if (diff_axis[i] < 3277) {
				acc_selftest |= 1 << i;

				if (bmi_get_err_status(client_data) < 0)
					return err;
				dev_err(client_data->dev,
					"Over selftest minimum for "
					"axis:%s,diff=%d, p_val=%d, n_val=%d\n",
					bmi_axis_name[i], diff_axis[i],
						axis_p_value, axis_n_value);
				dev_err(client_data->dev, "err_st:0x%x\n",
						client_data->err_st.err_st_all);

			}
		}

	}
	/* gyro_selftest==1,gyro selftest successfully,
	* but bmi_result bit4 0 is successful, 1 is failed*/
	bmi_selftest = (acc_selftest & 0x0f) | ((!gyro_selftest) << AXIS_MAX);
	atomic_set(&client_data->selftest_result, bmi_selftest);
	/*soft reset*/
	err = BMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	if (err)
		return err;
	msleep(50);
	dev_notice(client_data->dev, "Selftest for BMI16x finished\n");
	return count;
}

/* gyro sensor part */
static ssize_t bmi160_gyro_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	int err = 0;
	u8 gyro_pmu_status = 0;

	err = BMI_CALL_API(get_gyro_power_mode_stat)(
		&gyro_pmu_status);

	if (err)
		return err;
	else
	return snprintf(buf, 32, "reg:%d, val:%d\n", gyro_pmu_status,
				client_data->pw.gyro_pm);
}

static ssize_t bmi160_gyro_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	unsigned long op_mode;
	int err;

	err = bmi160_check_gyro_early_buff_enable_flag(client_data);
	if (err)
		return count;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	mutex_lock(&client_data->mutex_op_mode);

	if (op_mode < BMI_GYRO_PM_MAX) {
		switch (op_mode) {
		case BMI_GYRO_PM_NORMAL:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_NORMAL]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_NORMAL;
			bmi_delay(60);
			break;
		case BMI_GYRO_PM_FAST_START:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_FAST_START]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_FAST_START;
			bmi_delay(60);
			break;
		case BMI_GYRO_PM_SUSPEND:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_SUSPEND]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_SUSPEND;
			bmi_delay(60);
			break;
		default:
			mutex_unlock(&client_data->mutex_op_mode);
			return -EINVAL;
		}
	} else {
		mutex_unlock(&client_data->mutex_op_mode);
		return -EINVAL;
	}

	mutex_unlock(&client_data->mutex_op_mode);

	if (err)
		return err;
	else
		return count;

}

static ssize_t bmi160_gyro_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi160_gyro_t data;
	int err;

	err = BMI_CALL_API(read_gyro_xyz)(&data);
	if (err < 0)
		return err;


	return snprintf(buf, 48, "%hd %hd %hd\n", data.x,
				data.y, data.z);
}

static ssize_t bmi160_gyro_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char range;
	err = BMI_CALL_API(get_gyro_range)(&range);
	if (err)
		return err;
	return snprintf(buf, 16, "%d\n", range);
}

static ssize_t bmi160_gyro_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	err = bmi160_check_gyro_early_buff_enable_flag(client_data);
	if (err)
		return count;

	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_range)(range);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_gyro_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char gyro_odr;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	err = BMI_CALL_API(get_gyro_output_data_rate)(&gyro_odr);
	if (err)
		return err;

	client_data->odr.gyro_odr = gyro_odr;
	return snprintf(buf, 16, "%d\n", gyro_odr);
}

static ssize_t bmi160_gyro_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long gyro_odr;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	err = bmi160_check_gyro_early_buff_enable_flag(client_data);
	if (err)
		return count;

	err = kstrtoul(buf, 10, &gyro_odr);
	if (err)
		return err;

	if (gyro_odr < 6 || gyro_odr > 13)
		return -EIO;

	err = BMI_CALL_API(set_gyro_output_data_rate)(gyro_odr);
	if (err)
		return -EIO;

	client_data->odr.gyro_odr = gyro_odr;
	return count;
}

static ssize_t bmi160_gyro_fast_calibration_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_gyro_enable)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_gyro_fast_calibration_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long enable;
	s8 err;
	s16 gyr_off_x;
	s16 gyr_off_y;
	s16 gyr_off_z;

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;

	err = BMI_CALL_API(set_foc_gyro_enable)((u8)enable,
				&gyr_off_x, &gyr_off_y, &gyr_off_z);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_gyro_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data = 0;
	s8 err = 0;

	err = BMI_CALL_API(get_gyro_offset_compensation_xaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_gyro_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	s8 err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_offset_compensation_xaxis)((s16)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_gyro_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data = 0;
	s8 err = 0;

	err = BMI_CALL_API(get_gyro_offset_compensation_yaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_gyro_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	s8 err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_offset_compensation_yaxis)((s16)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_gyro_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data = 0;
	int err = 0;

	err = BMI_CALL_API(get_gyro_offset_compensation_zaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_gyro_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_offset_compensation_zaxis)((s16)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_fifo_data_out_frame_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	int err = 0;
	uint32_t fifo_bytecount = 0;

	err = BMI_CALL_API(fifo_length)(&fifo_bytecount);
	if (err < 0) {
		dev_err(client_data->dev, "read fifo_length err");
		return -EINVAL;
	}
	if (fifo_bytecount == 0)
		return 0;
	err = bmi_i2c_read(client_data->device.dev_addr,
		BMI160_USER_FIFO_DATA__REG, buf,
		fifo_bytecount);
	if (err) {
		dev_err(client_data->dev, "read fifo err");
		BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);
		return -EINVAL;
	}
	return fifo_bytecount;
}

/* mag sensor part */
#ifdef BMI160_MAG_INTERFACE_SUPPORT

static ssize_t bmi160_mag_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	u8 mag_op_mode;
	s8 err;
	err = bmi160_get_mag_power_mode_stat(&mag_op_mode);
	if (err) {
		dev_err(client_data->dev,
			"Failed to get BMI160 mag power mode:%d\n", err);
		return err;
	} else
		return snprintf(buf, 32, "%d, reg:%d\n",
					client_data->pw.mag_pm, mag_op_mode);
}

static ssize_t bmi160_mag_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	unsigned long op_mode;
	int err;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	if (op_mode == client_data->pw.mag_pm)
		return count;

	mutex_lock(&client_data->mutex_op_mode);


	if (op_mode < BMI_MAG_PM_MAX) {
		switch (op_mode) {
		case BMI_MAG_PM_NORMAL:
			/* need to modify as mag sensor connected,
			 * set write address to 0x4c and triggers
			 * write operation
			 * 0x4c(op mode control reg)
			 * enables normal mode in magnetometer */
#if defined(BMI160_AKM09912_SUPPORT)
			err = bmi160_set_bst_akm_and_secondary_if_powermode(
			BMI160_MAG_FORCE_MODE);
#else
			err = bmi160_set_bmm150_mag_and_secondary_if_power_mode(
			BMI160_MAG_FORCE_MODE);
#endif
			client_data->pw.mag_pm = BMI_MAG_PM_NORMAL;
			bmi_delay(5);
			break;
		case BMI_MAG_PM_LP1:
			/* need to modify as mag sensor connected,
			 * set write address to 0x4 band triggers
			 * write operation
			 * 0x4b(bmm150, power control reg, bit0)
			 * enables power in magnetometer*/
#if defined(BMI160_AKM09912_SUPPORT)
			err = bmi160_set_bst_akm_and_secondary_if_powermode(
			BMI160_MAG_FORCE_MODE);
#else
			err = bmi160_set_bmm150_mag_and_secondary_if_power_mode(
			BMI160_MAG_FORCE_MODE);
#endif
			client_data->pw.mag_pm = BMI_MAG_PM_LP1;
			bmi_delay(5);
			break;
		case BMI_MAG_PM_SUSPEND:
		case BMI_MAG_PM_LP2:
#if defined(BMI160_AKM09912_SUPPORT)
		err = bmi160_set_bst_akm_and_secondary_if_powermode(
		BMI160_MAG_SUSPEND_MODE);
#else
		err = bmi160_set_bmm150_mag_and_secondary_if_power_mode(
		BMI160_MAG_SUSPEND_MODE);
#endif
			client_data->pw.mag_pm = op_mode;
			bmi_delay(5);
			break;
		default:
			mutex_unlock(&client_data->mutex_op_mode);
			return -EINVAL;
		}
	} else {
		mutex_unlock(&client_data->mutex_op_mode);
		return -EINVAL;
	}

	mutex_unlock(&client_data->mutex_op_mode);

	if (err) {
		dev_err(client_data->dev,
			"Failed to switch BMI160 mag power mode:%d\n",
			client_data->pw.mag_pm);
		return err;
	} else
		return count;

}

static ssize_t bmi160_mag_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char mag_odr = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);


	err = BMI_CALL_API(get_mag_output_data_rate)(&mag_odr);
	if (err)
		return err;

	client_data->odr.mag_odr = mag_odr;
	return snprintf(buf, 16, "%d\n", mag_odr);
}

static ssize_t bmi160_mag_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long mag_odr;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	err = kstrtoul(buf, 10, &mag_odr);
	if (err)
		return err;
	/*1~25/32hz,..6(25hz),7(50hz),...9(200hz) */
	err = BMI_CALL_API(set_mag_output_data_rate)(mag_odr);
	if (err)
		return -EIO;

	client_data->odr.mag_odr = mag_odr;
	return count;
}

static ssize_t bmi160_mag_i2c_address_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 data;
	s8 err;

	err = BMI_CALL_API(set_mag_manual_enable)(1);
	err += BMI_CALL_API(get_i2c_device_addr)(&data);
	err += BMI_CALL_API(set_mag_manual_enable)(0);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "0x%x\n", data);
}

static ssize_t bmi160_mag_i2c_address_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err += BMI_CALL_API(set_mag_manual_enable)(1);
	if (!err)
		err += BMI_CALL_API(set_i2c_device_addr)((unsigned char)data);
	err += BMI_CALL_API(set_mag_manual_enable)(0);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_mag_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	struct bmi160_mag_xyz_s32_t data;
	int err;
	/* raw data with compensation */
#if defined(BMI160_AKM09912_SUPPORT)
	err = bmi160_bst_akm09912_compensate_xyz(&data);
#else
	err = bmi160_bmm150_mag_compensate_xyz(&data);
#endif

	if (err < 0) {
		memset(&data, 0, sizeof(data));
		dev_err(client_data->dev, "mag not ready!\n");
	}
	return snprintf(buf, 48, "%hd %hd %hd\n", data.x,
				data.y, data.z);
}
static ssize_t bmi160_mag_offset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char mag_offset;
	err = BMI_CALL_API(get_mag_offset)(&mag_offset);
	if (err)
		return err;

	return snprintf(buf, 16, "%d\n", mag_offset);

}
static ssize_t bmi160_mag_offset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err += BMI_CALL_API(set_mag_manual_enable)(1);
	if (err == 0)
		err += BMI_CALL_API(set_mag_offset)((unsigned char)data);
	err += BMI_CALL_API(set_mag_manual_enable)(0);

	if (err < 0)
		return -EIO;
	return count;
}
static ssize_t bmi160_mag_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s8 err = 0;
	u8 mag_chipid;

	err = bmi160_set_mag_manual_enable(0x01);
	/* read mag chip_id value */
#if defined(BMI160_AKM09912_SUPPORT)
	err += bmi160_set_mag_read_addr(AKM09912_CHIP_ID_REG);
		/* 0x04 is mag_x lsb register */
	err += bmi160_read_reg(BMI160_USER_DATA_0_MAG_X_LSB__REG,
							&mag_chipid, 1);
	/* Must add this commands to re-set data register addr of mag sensor */
	err += bmi160_set_mag_read_addr(AKM_DATA_REGISTER);
#else
	err += bmi160_set_mag_read_addr(BMI160_BMM150_CHIP_ID);
	/* 0x04 is mag_x lsb register */
	err += bmi160_read_reg(BMI160_USER_DATA_0_MAG_X_LSB__REG,
							&mag_chipid, 1);

	/* Must add this commands to re-set data register addr of mag sensor */
	/* 0x42 is  bmm150 data register address */
	err += bmi160_set_mag_read_addr(BMI160_BMM150_DATA_REG);
#endif
	err += bmi160_set_mag_manual_enable(0x00);

	if (err)
		return err;

	return snprintf(buf, 16, "chip_id:0x%x\n", mag_chipid);

}

struct bmi160_mag_xyz_s32_t mag_compensate;
static ssize_t bmi160_mag_compensate_xyz_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	memcpy(buf, &mag_compensate, sizeof(mag_compensate));
	if (debug_level & 0x04)
		printk(KERN_INFO "%hd %hd %hd\n",
		mag_compensate.x, mag_compensate.y, mag_compensate.z);
	return sizeof(mag_compensate);
}
static ssize_t bmi160_mag_compensate_xyz_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct bmi160_mag_xyzr_t mag_raw;
	memset(&mag_compensate, 0, sizeof(mag_compensate));
	memset(&mag_raw, 0, sizeof(mag_raw));
	mag_raw.x = (buf[1] << 8 | buf[0]);
	mag_raw.y = (buf[3] << 8 | buf[2]);
	mag_raw.z = (buf[5] << 8 | buf[4]);
	mag_raw.r = (buf[7] << 8 | buf[6]);
	mag_raw.x = mag_raw.x >> 3;
	mag_raw.y = mag_raw.y >> 3;
	mag_raw.z = mag_raw.z >> 1;
	mag_raw.r = mag_raw.r >> 2;
	bmi160_bmm150_mag_compensate_xyz_raw(
	&mag_compensate, mag_raw);
	return count;
}

#endif

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static ssize_t bmi_enable_int_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int interrupt_type, value;

	if (bmi160_check_acc_gyro_early_buff_enable_flag())
		return count;

	sscanf(buf, "%3d %3d", &interrupt_type, &value);

	if (interrupt_type < 0 || interrupt_type > 16)
		return -EINVAL;

	if (interrupt_type <= BMI_FLAT_INT) {
		if (BMI_CALL_API(set_intr_enable_0)
				(bmi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	} else if (interrupt_type <= BMI_FWM_INT) {
		if (BMI_CALL_API(set_intr_enable_1)
			(bmi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	} else {
		if (BMI_CALL_API(set_intr_enable_2)
			(bmi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	}
	return count;
}

#endif

static ssize_t bmi160_show_reg_sel(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	return snprintf(buf, 64, "reg=0X%02X, len=%d\n",
		client_data->reg_sel, client_data->reg_len);
}

static ssize_t bmi160_store_reg_sel(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	ssize_t ret;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}
	ret = sscanf(buf, "%11X %11d",
		&client_data->reg_sel, &client_data->reg_len);
	if (ret != 2) {
		dev_err(client_data->dev, "Invalid argument");
		return -EINVAL;
	}

	return count;
}

static ssize_t bmi160_show_reg_val(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	ssize_t ret;
	u8 reg_data[128], i;
	int pos;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	ret = bmi_i2c_read(client_data->device.dev_addr,
		client_data->reg_sel,
		reg_data, client_data->reg_len);
	if (ret < 0) {
		dev_err(client_data->dev, "Reg op failed");
		return ret;
	}

	pos = 0;
	for (i = 0; i < client_data->reg_len; ++i) {
		pos += snprintf(buf + pos, 16, "%02X", reg_data[i]);
		buf[pos++] = (i + 1) % 16 == 0 ? '\n' : ' ';
	}
	if (buf[pos - 1] == ' ')
		buf[pos - 1] = '\n';

	return pos;
}

static ssize_t bmi160_store_reg_val(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	ssize_t ret;
	u8 reg_data[32];
	int i, j, status, digit;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}
	status = 0;
	for (i = j = 0; i < count && j < client_data->reg_len; ++i) {
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' ||
			buf[i] == '\r') {
			status = 0;
			++j;
			continue;
		}
		digit = buf[i] & 0x10 ? (buf[i] & 0xF) : ((buf[i] & 0xF) + 9);
		printk(KERN_INFO "digit is %d", digit);
		switch (status) {
		case 2:
			++j; /* Fall thru */
		case 0:
			reg_data[j] = digit;
			status = 1;
			break;
		case 1:
			reg_data[j] = reg_data[j] * 16 + digit;
			status = 2;
			break;
		}
	}
	if (status > 0)
		++j;
	if (j > client_data->reg_len)
		j = client_data->reg_len;
	else if (j < client_data->reg_len) {
		dev_err(client_data->dev, "Invalid argument");
		return -EINVAL;
	}
	printk(KERN_INFO "Reg data read as");
	for (i = 0; i < j; ++i)
		printk(KERN_INFO "%d", reg_data[i]);

	ret = bmi_i2c_write(client_data->device.dev_addr,
		client_data->reg_sel,
		reg_data, client_data->reg_len);
	if (ret < 0) {
		dev_err(client_data->dev, "Reg op failed");
		return ret;
	}

	return count;
}

static ssize_t bmi160_driver_version_show(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	int ret;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	ret = snprintf(buf, 128, "Driver version: %s\n",
			DRIVER_VERSION);

	return ret;
}

static int bmi_read_axis_data(struct iio_dev *indio_dev, u8 reg_address,
		s16 *data)
{
	int ret;
	u8 v_data_u8r[2] = {0, 0};
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	ret = bmi_i2c_read(client_data->device.dev_addr,
		reg_address, v_data_u8r, 2);
	if (ret < 0)
		return ret;
	*data = (s16)
			((((s16)((s8)v_data_u8r[1]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | (
			v_data_u8r[0]));
	return 0;
}

static int bmi_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *ch, int *val,
							int *val2, long mask)
{
	int ret, result;
	s16 tval = 0;
	switch (mask) {
	case 0:
	{
		result = 0;
		ret = IIO_VAL_INT;
		mutex_lock(&indio_dev->mlock);
		switch (ch->type) {
		case IIO_ACCEL:
			result = bmi_read_axis_data(indio_dev,
						ch->address, &tval);
			*val = tval;
			break;
		case IIO_ANGL_VEL:
			result = bmi_read_axis_data(indio_dev,
							ch->address, &tval);
			*val = tval;
			break;
#ifdef BMI160_MAG_INTERFACE_SUPPORT
		case IIO_MAGN:
			result = bmi_read_axis_data(indio_dev,
							ch->address, &tval);
			*val = tval;
			break;
#endif
		default:
			ret = -EINVAL;
			break;
		}
	mutex_unlock(&indio_dev->mlock);
	if (result < 0)
		return result;
	return ret;
	}

	case IIO_CHAN_INFO_SCALE:
	{
		switch (ch->type) {
		case IIO_ANGL_VEL:
			*val = 0;
			*val2 = 1000000;
			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_TEMP:
			*val = 0;
			*val2 = BMI_TEMP_SCALE;
			return IIO_VAL_INT_PLUS_MICRO;
		default:
			return -EINVAL;
		}
	}

	case IIO_CHAN_INFO_OFFSET:
	{
		switch (ch->type) {
		case IIO_TEMP:
			*val = BMI_TEMP_OFFSET;
			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}
	}

	default:
		return -EINVAL;
	}

}


static IIO_DEVICE_ATTR(chip_id, S_IRUGO,
	bmi160_chip_id_show, NULL, 0);
#ifdef CONFIG_ENABLE_ACC_GYRO_BUFFERING
static IIO_DEVICE_ATTR(read_acc_boot_sample, S_IRUGO | S_IWUSR,
		read_acc_boot_sample_show, read_acc_boot_sample_store, 0);
static IIO_DEVICE_ATTR(read_gyro_boot_sample, S_IRUGO | S_IWUSR,
		read_gyro_boot_sample_show, read_gyro_boot_sample_store, 0);
#endif
static IIO_DEVICE_ATTR(err_st, S_IRUGO,
	bmi160_err_st_show, NULL, 0);
static IIO_DEVICE_ATTR(sensor_time, S_IRUGO,
	bmi160_sensor_time_show, NULL, 0);
static IIO_DEVICE_ATTR(selftest,  S_IRUGO | S_IWUSR,
	bmi160_selftest_show, bmi160_selftest_store, 0);
static IIO_DEVICE_ATTR(fifo_flush, S_IRUGO | S_IWUSR,
	NULL, bmi160_fifo_flush_store, 0);
static IIO_DEVICE_ATTR(fifo_bytecount, S_IRUGO | S_IWUSR,
	bmi160_fifo_bytecount_show, bmi160_fifo_bytecount_store, 0);
static IIO_DEVICE_ATTR(fifo_data_sel, S_IRUGO | S_IWUSR,
	bmi160_fifo_data_sel_show, bmi160_fifo_data_sel_store, 0);
static IIO_DEVICE_ATTR(fifo_watermark, S_IRUGO | S_IWUSR,
	bmi160_fifo_watermark_show, bmi160_fifo_watermark_store, 0);
static IIO_DEVICE_ATTR(fifo_header_en, S_IRUGO | S_IWUSR,
	bmi160_fifo_header_en_show, bmi160_fifo_header_en_store, 0);
static IIO_DEVICE_ATTR(fifo_time_en, S_IRUGO | S_IWUSR,
	bmi160_fifo_time_en_show, bmi160_fifo_time_en_store, 0);
static IIO_DEVICE_ATTR(fifo_int_tag_en, S_IRUGO | S_IWUSR,
	bmi160_fifo_int_tag_en_show, bmi160_fifo_int_tag_en_store, 0);
static IIO_DEVICE_ATTR(temperature, S_IRUGO,
	bmi160_temperature_show, NULL, 0);
static IIO_DEVICE_ATTR(acc_range, S_IRUGO | S_IWUSR,
	bmi160_acc_range_show, bmi160_acc_range_store, 0);
static IIO_DEVICE_ATTR(acc_odr, S_IRUGO | S_IWUSR,
	bmi160_acc_odr_show, bmi160_acc_odr_store, 0);
static IIO_DEVICE_ATTR(acc_op_mode, S_IRUGO | S_IWUSR,
	bmi160_acc_op_mode_show, bmi160_acc_op_mode_store, 0);
static IIO_DEVICE_ATTR(acc_value, S_IRUGO,
	bmi160_acc_value_show, NULL, 0);
static IIO_DEVICE_ATTR(acc_fast_calibration_x, S_IRUGO | S_IWUSR,
	bmi160_acc_fast_calibration_x_show,
	bmi160_acc_fast_calibration_x_store, 0);
static IIO_DEVICE_ATTR(acc_fast_calibration_y, S_IRUGO | S_IWUSR,
	bmi160_acc_fast_calibration_y_show,
	bmi160_acc_fast_calibration_y_store, 0);
static IIO_DEVICE_ATTR(acc_fast_calibration_z, S_IRUGO | S_IWUSR,
	bmi160_acc_fast_calibration_z_show,
	bmi160_acc_fast_calibration_z_store, 0);
static IIO_DEVICE_ATTR(acc_offset_x, S_IRUGO | S_IWUSR,
	bmi160_acc_offset_x_show,
	bmi160_acc_offset_x_store, 0);
static IIO_DEVICE_ATTR(acc_offset_y, S_IRUGO | S_IWUSR,
	bmi160_acc_offset_y_show,
	bmi160_acc_offset_y_store, 0);
static IIO_DEVICE_ATTR(acc_offset_z, S_IRUGO | S_IWUSR,
	bmi160_acc_offset_z_show,
	bmi160_acc_offset_z_store, 0);
static IIO_DEVICE_ATTR(test, S_IRUGO,
	bmi160_test_show, NULL, 0);
static IIO_DEVICE_ATTR(stc_enable, S_IRUGO | S_IWUSR,
	bmi160_step_counter_enable_show,
	bmi160_step_counter_enable_store, 0);
static IIO_DEVICE_ATTR(stc_mode, S_IRUGO | S_IWUSR,
	NULL, bmi160_step_counter_mode_store, 0);
static IIO_DEVICE_ATTR(stc_clc, S_IRUGO | S_IWUSR,
	NULL, bmi160_step_counter_clc_store, 0);
static IIO_DEVICE_ATTR(stc_value, S_IRUGO,
	bmi160_step_counter_value_show, NULL, 0);
static IIO_DEVICE_ATTR(reg_sel, S_IRUGO | S_IWUSR,
	bmi160_show_reg_sel, bmi160_store_reg_sel, 0);
static IIO_DEVICE_ATTR(reg_val, S_IRUGO | S_IWUSR,
	bmi160_show_reg_val, bmi160_store_reg_val, 0);
static IIO_DEVICE_ATTR(driver_version, S_IRUGO,
	bmi160_driver_version_show, NULL, 0);
#ifdef BMI160_DEBUG
static IIO_DEVICE_ATTR(debug_level, S_IRUGO | S_IWUSR,
	bmi160_debug_level_show,
	bmi160_debug_level_store, 0);
#endif
/* gyro part */
static IIO_DEVICE_ATTR(gyro_op_mode, S_IRUGO | S_IWUSR,
	bmi160_gyro_op_mode_show, bmi160_gyro_op_mode_store, 0);
static IIO_DEVICE_ATTR(gyro_value, S_IRUGO,
	bmi160_gyro_value_show, NULL, 0);
static IIO_DEVICE_ATTR(gyro_range, S_IRUGO | S_IWUSR,
	bmi160_gyro_range_show, bmi160_gyro_range_store, 0);
static IIO_DEVICE_ATTR(gyro_odr, S_IRUGO | S_IWUSR,
	bmi160_gyro_odr_show, bmi160_gyro_odr_store, 0);
static IIO_DEVICE_ATTR(gyro_fast_calibration_en,
	S_IRUGO | S_IWUSR,
	bmi160_gyro_fast_calibration_en_show,
	bmi160_gyro_fast_calibration_en_store, 0);
static IIO_DEVICE_ATTR(gyro_offset_x, S_IRUGO | S_IWUSR,
	bmi160_gyro_offset_x_show, bmi160_gyro_offset_x_store, 0);
static IIO_DEVICE_ATTR(gyro_offset_y, S_IRUGO | S_IWUSR,
	bmi160_gyro_offset_y_show, bmi160_gyro_offset_y_store, 0);
static IIO_DEVICE_ATTR(gyro_offset_z, S_IRUGO | S_IWUSR,
	bmi160_gyro_offset_z_show, bmi160_gyro_offset_z_store, 0);
static IIO_DEVICE_ATTR(fifo_data_frame, S_IRUGO,
	bmi160_fifo_data_out_frame_show, NULL, 0);

#ifdef BMI160_MAG_INTERFACE_SUPPORT
static IIO_DEVICE_ATTR(mag_op_mode, S_IRUGO | S_IWUSR,
	bmi160_mag_op_mode_show, bmi160_mag_op_mode_store, 0);
static IIO_DEVICE_ATTR(mag_odr, S_IRUGO | S_IWUSR,
	bmi160_mag_odr_show, bmi160_mag_odr_store, 0);
static IIO_DEVICE_ATTR(mag_i2c_addr, S_IRUGO | S_IWUSR,
	bmi160_mag_i2c_address_show, bmi160_mag_i2c_address_store, 0);
static IIO_DEVICE_ATTR(mag_value, S_IRUGO,
	bmi160_mag_value_show, NULL, 0);
static IIO_DEVICE_ATTR(mag_offset, S_IRUGO | S_IWUSR,
	bmi160_mag_offset_show, bmi160_mag_offset_store, 0);
static IIO_DEVICE_ATTR(mag_chip_id, S_IRUGO,
	bmi160_mag_chip_id_show, NULL, 0);
static IIO_DEVICE_ATTR(mag_compensate, S_IRUGO | S_IWUSR,
	bmi160_mag_compensate_xyz_show,
	bmi160_mag_compensate_xyz_store, 0);

#endif
#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static IIO_DEVICE_ATTR(enable_int, S_IRUGO | S_IWUSR,
	NULL, bmi_enable_int_store, 0);
static IIO_DEVICE_ATTR(anymot_duration, S_IRUGO | S_IWUSR,
	bmi160_anymot_duration_show, bmi160_anymot_duration_store, 0);
static IIO_DEVICE_ATTR(anymot_threshold, S_IRUGO | S_IWUSR,
	bmi160_anymot_threshold_show, bmi160_anymot_threshold_store, 0);
static IIO_DEVICE_ATTR(std_stu, S_IRUGO,
	bmi160_step_detector_status_show, NULL, 0);
static IIO_DEVICE_ATTR(std_en, S_IRUGO | S_IWUSR,
	bmi160_step_detector_enable_show,
	bmi160_step_detector_enable_store, 0);
static IIO_DEVICE_ATTR(sig_en, S_IRUGO | S_IWUSR,
	bmi160_signification_motion_enable_show,
	bmi160_signification_motion_enable_store, 0);
static IIO_DEVICE_ATTR(sig_value, S_IRUGO,
	bmi160_sig_motion_value_show, NULL, 0);
#endif
static IIO_DEVICE_ATTR(bmi_value, S_IRUGO,
	bmi160_bmi_value_show, NULL, 0);

static struct attribute *bmi_attributes[] = {
	&iio_dev_attr_chip_id.dev_attr.attr,
#ifdef CONFIG_ENABLE_ACC_GYRO_BUFFERING
	&iio_dev_attr_read_acc_boot_sample.dev_attr.attr,
	&iio_dev_attr_read_gyro_boot_sample.dev_attr.attr,
#endif
	&iio_dev_attr_err_st.dev_attr.attr,
	&iio_dev_attr_sensor_time.dev_attr.attr,
	&iio_dev_attr_selftest.dev_attr.attr,
	&iio_dev_attr_driver_version.dev_attr.attr,
	&iio_dev_attr_test.dev_attr.attr,
	&iio_dev_attr_fifo_flush.dev_attr.attr,
	&iio_dev_attr_fifo_header_en.dev_attr.attr,
	&iio_dev_attr_fifo_time_en.dev_attr.attr,
	&iio_dev_attr_fifo_int_tag_en.dev_attr.attr,
	&iio_dev_attr_fifo_bytecount.dev_attr.attr,
	&iio_dev_attr_fifo_data_sel.dev_attr.attr,
	&iio_dev_attr_fifo_watermark.dev_attr.attr,
	&iio_dev_attr_temperature.dev_attr.attr,
	&iio_dev_attr_acc_range.dev_attr.attr,
	&iio_dev_attr_acc_odr.dev_attr.attr,
	&iio_dev_attr_acc_op_mode.dev_attr.attr,
	&iio_dev_attr_acc_value.dev_attr.attr,
	&iio_dev_attr_acc_fast_calibration_x.dev_attr.attr,
	&iio_dev_attr_acc_fast_calibration_y.dev_attr.attr,
	&iio_dev_attr_acc_fast_calibration_z.dev_attr.attr,
	&iio_dev_attr_acc_offset_x.dev_attr.attr,
	&iio_dev_attr_acc_offset_y.dev_attr.attr,
	&iio_dev_attr_acc_offset_z.dev_attr.attr,
	&iio_dev_attr_stc_enable.dev_attr.attr,
	&iio_dev_attr_stc_mode.dev_attr.attr,
	&iio_dev_attr_stc_clc.dev_attr.attr,
	&iio_dev_attr_stc_value.dev_attr.attr,
	&iio_dev_attr_gyro_op_mode.dev_attr.attr,
	&iio_dev_attr_gyro_value.dev_attr.attr,
	&iio_dev_attr_gyro_range.dev_attr.attr,
	&iio_dev_attr_gyro_odr.dev_attr.attr,
	&iio_dev_attr_gyro_fast_calibration_en.dev_attr.attr,
	&iio_dev_attr_gyro_offset_x.dev_attr.attr,
	&iio_dev_attr_gyro_offset_y.dev_attr.attr,
	&iio_dev_attr_gyro_offset_z.dev_attr.attr,
	&iio_dev_attr_fifo_data_frame.dev_attr.attr,
#ifdef BMI160_DEBUG
	&iio_dev_attr_debug_level.dev_attr.attr,
#endif
#ifdef BMI160_MAG_INTERFACE_SUPPORT
	&iio_dev_attr_mag_chip_id.dev_attr.attr,
	&iio_dev_attr_mag_op_mode.dev_attr.attr,
	&iio_dev_attr_mag_odr.dev_attr.attr,
	&iio_dev_attr_mag_i2c_addr.dev_attr.attr,
	&iio_dev_attr_mag_value.dev_attr.attr,
	&iio_dev_attr_mag_offset.dev_attr.attr,
	&iio_dev_attr_mag_compensate.dev_attr.attr,
#endif
#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
	&iio_dev_attr_enable_int.dev_attr.attr,
	&iio_dev_attr_anymot_duration.dev_attr.attr,
	&iio_dev_attr_anymot_threshold.dev_attr.attr,
	&iio_dev_attr_std_stu.dev_attr.attr,
	&iio_dev_attr_std_en.dev_attr.attr,
	&iio_dev_attr_sig_en.dev_attr.attr,
	&iio_dev_attr_sig_value.dev_attr.attr,
#endif
	&iio_dev_attr_reg_sel.dev_attr.attr,
	&iio_dev_attr_reg_val.dev_attr.attr,
	&iio_dev_attr_bmi_value.dev_attr.attr,
	NULL,
};

static const struct attribute_group bmi_attribute_group = {
	.attrs = bmi_attributes,
};

static const struct iio_info bmiacc_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &bmi_attribute_group,
	.read_raw = &bmi_read_raw,
};

static const struct iio_info bmigyro_iio_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &bmi_read_raw,
};
static const struct iio_info bmimagn_iio_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &bmi_read_raw,
};

static unsigned char fifo_data[1024];

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static void bmi_signification_motion_interrupt_handle(
	struct bmi_client_data *client_data)
{
	client_data->sig_flag = 1;
}

static void bmi_stepdetector_interrupt_handle(
	struct bmi_client_data *client_data)
{
	u8 current_step_dector_st = 0;
	client_data->pedo_data.wkar_step_detector_status++;
	current_step_dector_st =
		client_data->pedo_data.wkar_step_detector_status;
	client_data->std = ((current_step_dector_st == 1) ? 0 : 1);
}
static void bmi_fifo_watermark_interrupt_handle(
	struct bmi_client_data *client_data)
{
	int err = 0;
	unsigned int fifo_len0 = 0;
	unsigned int fifo_frmbytes_ext = 0;

	err = BMI_CALL_API(fifo_length)(&fifo_len0);
	client_data->fifo_bytecount = fifo_len0;

	if (client_data->fifo_bytecount == 0 || err) {
		dev_err(client_data->dev, "read fifo leght zero or err");
		BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);
		return;
	}

	if (client_data->fifo_bytecount + fifo_frmbytes_ext > FIFO_DATA_BUFSIZE)
		client_data->fifo_bytecount = FIFO_DATA_BUFSIZE;
	memset(fifo_data, 0, 1024);
	err = bmi_i2c_read(client_data->device.dev_addr,
		BMI160_USER_FIFO_DATA__REG, fifo_data,
		client_data->fifo_bytecount);
	if (err) {
		BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);
		dev_err(client_data->dev, "read fifo err");
		return;
	}
	err = bmi_fifo_analysis_handle(client_data, fifo_data,
		client_data->fifo_bytecount);
	if (err)
		dev_err(client_data->dev,
			"analysis handle failed:%d\n", err);
	/*check again the fifo length*/
	do {
		err = BMI_CALL_API(fifo_length)(&fifo_len0);
		if (err) {
			err = BMI_CALL_API(set_command_register)(
			CMD_CLR_FIFO_DATA);
			dev_err(client_data->dev,
			"after interrupt read fifo length err");
			return;
		} else {
		/*check after the interrupt the
		fifo_length bigger than watermark*/
			if (fifo_len0 > 40) {
				dev_err(client_data->dev,
				"have %d after interrupt read fifo", fifo_len0);
				err = BMI_CALL_API(set_command_register)(
				CMD_CLR_FIFO_DATA);
			}
			return;
		}
	} while (fifo_len0 > 0);
}

irqreturn_t bmi_irq_handler(int irq, void *dev_id)
{
	struct bmi_client_data *client_data = (struct bmi_client_data *)dev_id;
	uint64_t timestamp;
	uint64_t timestamp_ofday;
	timestamp = get_alarm_timestamp();
	timestamp_ofday = get_timeofday_timestamp();
	client_data->fifo_time = timestamp_ofday;
	client_data->alarm_time = timestamp;
	client_data->del_time = timestamp_ofday - timestamp;
	return IRQ_WAKE_THREAD;
}

static irqreturn_t bmi_report_handle(int irq, void *dev_id)
{
	struct bmi_client_data *client_data = (struct bmi_client_data *)dev_id;
	struct bmi160_accel_t acc_data;
	struct bmi160_gyro_t gyro_data;
	unsigned char int_status[4] = {0, 0, 0 , 0};
	unsigned char data_ready = 0;
	int err;
	/*
	client_data->device.bus_read(client_data->device.dev_addr,
				BMI160_USER_INTR_STAT_0_ADDR, int_status, 4);
	*/
	client_data->device.bus_read(client_data->device.dev_addr,
				BMI160_USER_STAT_ADDR, &data_ready, 1);
	/*printk(KERN_INFO "data_ready %x \n",data_ready);*/
	if (data_ready == 0x90) {
		err = BMI_CALL_API(read_accel_xyz)(&acc_data);
		bmi160_report_accel_data(accl_iio_private, 0,
			acc_data, client_data->alarm_time);
	}
	if (data_ready == 0x50) {
		err = BMI_CALL_API(read_gyro_xyz)(&gyro_data);
		bmi160_report_gyro_data(gyro_iio_private, 0,
			gyro_data, client_data->alarm_time);
	}
	if (data_ready == 0xD0) {
		err = BMI_CALL_API(read_accel_gyro_xyz)(&gyro_data, &acc_data);
		bmi160_report_accel_data(accl_iio_private, 0,
			acc_data, client_data->alarm_time);
		bmi160_report_gyro_data(gyro_iio_private, 0,
			gyro_data, client_data->alarm_time);
	}
	if (BMI160_GET_BITSLICE(int_status[0],
			BMI160_USER_INTR_STAT_0_STEP_INTR))
		bmi_stepdetector_interrupt_handle(client_data);
	if (BMI160_GET_BITSLICE(int_status[0],
			BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR))
		bmi_signification_motion_interrupt_handle(client_data);
	if (BMI160_GET_BITSLICE(int_status[1],
			BMI160_USER_INTR_STAT_1_FIFO_WM_INTR))
		bmi_fifo_watermark_interrupt_handle(client_data);

	return IRQ_HANDLED;
}
#endif
int bmi_probe(struct iio_dev *accl_iio_private,
				struct iio_dev *gyro_iio_private,
				struct iio_dev *magn_iio_private)
{
	int err = 0;
#ifdef BMI160_MAG_INTERFACE_SUPPORT
	u8 mag_dev_addr;
	u8 mag_urst_len;
	u8 mag_op_mode;
#endif
	struct device *p_i2c_dev;
	struct bmi_client_data *bmi160_private;

	bmi160_private = iio_priv(accl_iio_private);
	bmi160_private->device.delay_msec = bmi_delay;
	mutex_init(&bmi160_private->mutex_enable);
	mutex_init(&bmi160_private->mutex_op_mode);

	p_i2c_dev = accl_iio_private->dev.parent;

	err = bmi_check_chip_id(bmi160_private);
	if (err)
		goto exit_err_clean;

	/* h/w init */
	err = BMI_CALL_API(init)(&bmi160_private->device);
	if (err)
		dev_err(p_i2c_dev, "Failed soft reset, er=%d", err);

	bmi_dump_reg(bmi160_private);
	/*power on detected*/
	/*or softrest(cmd 0xB6) */
	/*fatal err check*/
	/*soft reset*/
	err = BMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	bmi_delay(3);
	if (err)
		dev_err(p_i2c_dev, "Failed soft reset, er=%d", err);
	/*usr data config page*/
	err = BMI_CALL_API(set_target_page)(USER_DAT_CFG_PAGE);
	if (err)
		dev_err(p_i2c_dev, "Failed cffg page, er=%d", err);
	err = bmi_get_err_status(bmi160_private);
	if (err) {
		dev_err(p_i2c_dev, "Failed to bmi16x init!err_st=0x%x\n",
				bmi160_private->err_st.err_st_all);
	}
#ifdef BMI160_MAG_INTERFACE_SUPPORT
	err += bmi160_set_command_register(MAG_MODE_NORMAL);
	bmi_delay(2);
	err += bmi160_get_mag_power_mode_stat(&mag_op_mode);
	bmi_delay(2);
	err += BMI_CALL_API(get_i2c_device_addr)(&mag_dev_addr);
	bmi_delay(2);
#if defined(BMI160_AKM09912_SUPPORT)
	err += BMI_CALL_API(set_i2c_device_addr)(BMI160_AKM09912_I2C_ADDRESS);
	bmi160_bst_akm_mag_interface_init(BMI160_AKM09912_I2C_ADDRESS);
#else
	err += BMI_CALL_API(set_i2c_device_addr)(
		BMI160_AUX_BMM150_I2C_ADDRESS);
	bmi160_bmm150_mag_interface_init();
#endif

	err += bmi160_set_mag_burst(3);
	err += bmi160_get_mag_burst(&mag_urst_len);
	if (err)
		dev_err(p_i2c_dev, "Failed cffg mag, er=%d", err);
	dev_info(p_i2c_dev,
		"BMI160 mag_urst_len:%d, mag_add:0x%x\n",
		mag_urst_len, mag_dev_addr);
#endif
#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
#ifdef BMI160_ENABLE_INT1
	/* maps interrupt to INT1/InT2 pin */
	/*err = BMI_CALL_API(set_int_anymo)(BMI_INT0, ENABLE);*/
	err = BMI_CALL_API(set_intr_fifo_wm)(BMI_INT0, ENABLE);
	err += BMI_CALL_API(set_intr_data_rdy)(BMI_INT0, ENABLE);
	/*Set interrupt trige level way */
	err += BMI_CALL_API(set_intr_edge_ctrl)(BMI_INT0, BMI_INT_LEVEL);
	err += bmi160_set_intr_level(BMI_INT0, 1);
	/*set interrupt latch temporary, 312.5us*/
	bmi160_set_latch_intr(1);
	err += BMI_CALL_API(set_output_enable)(
		BMI160_INTR1_OUTPUT_ENABLE, ENABLE);
	sigmotion_init_interrupts(BMI160_MAP_INTR1);
	err += BMI_CALL_API(map_step_detector_intr)(BMI160_MAP_INTR1);

	/*close step_detector in init function*/
	err += BMI_CALL_API(set_step_detector_enable)(0);
	if (err)
		dev_err(p_i2c_dev, "Failed set in1 er=%d", err);
#endif

#ifdef BMI160_ENABLE_INT2
	/* maps interrupt to INT1/InT2 pin */
	/*err = BMI_CALL_API(set_intr_any_motion)(BMI_INT1, ENABLE);*/
	err = BMI_CALL_API(set_intr_fifo_wm)(BMI_INT1, ENABLE);
	err += BMI_CALL_API(set_intr_data_rdy)(BMI_INT1, ENABLE);
	/*Set interrupt trige level way */
	err += BMI_CALL_API(set_intr_edge_ctrl)(BMI_INT1, BMI_INT_LEVEL);
	bmi160_set_intr_level(BMI_INT1, 1);
	/*set interrupt latch temporary, 312.5us*/
	bmi160_set_latch_intr(1);
	err += BMI_CALL_API(set_output_enable)(
		BMI160_INTR2_OUTPUT_ENABLE, ENABLE);
	sigmotion_init_interrupts(BMI160_MAP_INTR2);
	err += BMI_CALL_API(map_step_detector_intr)(BMI160_MAP_INTR2);

		/*close step_detector in init function*/
	err += BMI_CALL_API(set_step_detector_enable)(0);
	if (err)
		dev_err(p_i2c_dev, "Failed set in2  er=%d", err);
#endif
#endif
	bmi160_private->fifo_data_sel = 0;
	bmi160_private->sig_flag = 0;
	bmi160_private->fifo_time = 0;
	bmi160_private->fifo_time2 = 0;

	err = BMI_CALL_API(get_accel_output_data_rate)(
		&bmi160_private->odr.acc_odr);
	err += BMI_CALL_API(get_gyro_output_data_rate)(
		&bmi160_private->odr.gyro_odr);
	err += BMI_CALL_API(get_mag_output_data_rate)(
		&bmi160_private->odr.mag_odr);
	if (err)
		dev_err(p_i2c_dev, "Failed get odr er=%d", err);
	BMI_CALL_API(set_fifo_time_enable)(1);
	if (err)
		dev_err(p_i2c_dev, "Failed set_fifo_time_en er=%d", err);
	/* now it's power on which is considered as resuming from suspend */

	/* set sensor PMU into suspend power mode for all */
	err = bmi_pmu_set_suspend(bmi160_private);
	if (err)
		dev_err(p_i2c_dev, "Failed bmi_pmu_set_suspend er=%d", err);
#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
	bmi160_private->gpio_pin = of_get_named_gpio_flags(
		accl_iio_private->dev.parent->of_node,
		"bmi,gpio_irq", 0, NULL);
	dev_err(p_i2c_dev, "BMI160 qpio number:%d\n",
				bmi160_private->gpio_pin);
	err += gpio_request_one(bmi160_private->gpio_pin,
				GPIOF_IN, "bmi160_int");
	err += gpio_direction_input(bmi160_private->gpio_pin);
	bmi160_private->IRQ = gpio_to_irq(
		bmi160_private->gpio_pin);
	if (err) {
		dev_err(p_i2c_dev, "can not request gpio to irq number\n");
		bmi160_private->gpio_pin = 0;
	}
	err = request_threaded_irq(bmi160_private->IRQ,
		bmi_irq_handler,
		bmi_report_handle,
		IRQF_TRIGGER_RISING, "bmi_irq", bmi160_private);
#endif
	bmi160_private = iio_priv(gyro_iio_private);
	bmi160_private->device.delay_msec = bmi_delay;
	mutex_init(&bmi160_private->mutex_enable);
	mutex_init(&bmi160_private->mutex_op_mode);

#ifdef BMI160_MAG_INTERFACE_SUPPORT
	bmi160_private = iio_priv(magn_iio_private);
	bmi160_private->device.delay_msec = bmi_delay;
	mutex_init(&bmi160_private->mutex_enable);
	mutex_init(&bmi160_private->mutex_op_mode);
#endif
	accl_iio_private->channels = accl_iio_channels;
	accl_iio_private->num_channels = ARRAY_SIZE(accl_iio_channels);
	accl_iio_private->info = &bmiacc_iio_info;
	accl_iio_private->modes = INDIO_DIRECT_MODE;

	gyro_iio_private->channels = gyro_iio_channels;
	gyro_iio_private->num_channels = ARRAY_SIZE(gyro_iio_channels);
	gyro_iio_private->info = &bmigyro_iio_info;
	gyro_iio_private->modes = INDIO_DIRECT_MODE;

#ifdef BMI160_MAG_INTERFACE_SUPPORT
	magn_iio_private->channels = magn_iio_channels;
	magn_iio_private->num_channels = ARRAY_SIZE(magn_iio_channels);
	magn_iio_private->info = &bmimagn_iio_info;
	magn_iio_private->modes = INDIO_DIRECT_MODE;
#endif

	err = bmi_allocate_ring(accl_iio_private);
	if (err) {
		dev_err(p_i2c_dev,
				"bmi configure buffer fail %d\n", err);
		bmi_deallocate_ring(gyro_iio_private);
		return err;
	}

	err = bmi_allocate_ring(gyro_iio_private);
	if (err) {
		dev_err(p_i2c_dev,
				"bmi configure buffer fail %d\n", err);
		bmi_deallocate_ring(gyro_iio_private);
		return err;
	}
#ifdef BMI160_MAG_INTERFACE_SUPPORT
	err = bmi_allocate_ring(magn_iio_private);
	if (err) {
		dev_err(p_i2c_dev,
				"bmi configure buffer fail %d\n", err);
		bmi_deallocate_ring(magn_iio_private);
		return err;
	}
#endif
	/*open the channel, time stamp channel is defined by
	IIO_CHAN_SOFT_TIMESTAMP, it is opened in IIO itself*/
	iio_scan_mask_set(accl_iio_private,
		accl_iio_private->buffer, BMI_SCAN_ACCL_X);
	iio_scan_mask_set(accl_iio_private,
		accl_iio_private->buffer, BMI_SCAN_ACCL_Y);
	iio_scan_mask_set(accl_iio_private,
		accl_iio_private->buffer, BMI_SCAN_ACCL_Z);

	iio_scan_mask_set(gyro_iio_private,
		gyro_iio_private->buffer, BMI_SCAN_GYRO_X);
	iio_scan_mask_set(gyro_iio_private,
		gyro_iio_private->buffer, BMI_SCAN_GYRO_Y);
	iio_scan_mask_set(gyro_iio_private,
		gyro_iio_private->buffer, BMI_SCAN_GYRO_Z);

#ifdef BMI160_MAG_INTERFACE_SUPPORT
	iio_scan_mask_set(magn_iio_private,
		magn_iio_private->buffer, BMI_SCAN_MAGN_X);
	iio_scan_mask_set(magn_iio_private,
		magn_iio_private->buffer, BMI_SCAN_MAGN_Y);
	iio_scan_mask_set(magn_iio_private,
		magn_iio_private->buffer, BMI_SCAN_MAGN_Z);
#endif
	err = iio_device_register(accl_iio_private);
	if (err) {
		dev_err(p_i2c_dev,
				"bmi IIO device register failed %d\n", err);
		goto bmi_probe_ring_error;
	}

	err = iio_device_register(gyro_iio_private);
	if (err) {
		dev_err(p_i2c_dev,
				"bmi IIO device register failed %d\n", err);
		goto bmi_probe_ring_error;
	}

#ifdef BMI160_MAG_INTERFACE_SUPPORT
	err = iio_device_register(magn_iio_private);
	if (err) {
		dev_err(p_i2c_dev,
				"bmi IIO device register failed %d\n", err);
		goto bmi_probe_ring_error;
	}
#endif

	err = bmi160_acc_gyro_early_buff_init();
	if (!err)
		goto bmi_probe_ring_error;

	dev_notice(p_i2c_dev, "sensor_time:%d, %d",
		sensortime_duration_tbl[0].ts_delat,
		sensortime_duration_tbl[0].ts_duration_lsb);
	dev_notice(p_i2c_dev, "sensor %s probed successfully", SENSOR_NAME);

	return 0;

bmi_probe_ring_error:
	bmi_deallocate_ring(accl_iio_private);
	bmi_deallocate_ring(gyro_iio_private);
	bmi_deallocate_ring(magn_iio_private);

exit_err_clean:
	return err;
}
EXPORT_SYMBOL(bmi_probe);

/*!
 * @brief remove bmi client
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
int bmi_remove(struct device *dev)
{
	int err = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	bmi160_acc_gyro_input_cleanup(client_data);

	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);

	if (NULL != client_data) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&client_data->early_suspend_handler);
#endif
		err = bmi_pmu_set_suspend(client_data);
		bmi_delay(5);
	}
	return err;
}
EXPORT_SYMBOL(bmi_remove);
int bmi_suspend(struct device *dev)
{
	int err = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);
	unsigned char stc_enable;
	unsigned char std_enable;
	dev_err(client_data->dev, "bmi suspend function entrance");

	BMI_CALL_API(get_step_counter_enable)(&stc_enable);
	BMI_CALL_API(get_step_detector_enable)(&std_enable);
	if (client_data->pw.acc_pm != BMI_ACC_PM_SUSPEND &&
		(stc_enable != 1) && (std_enable != 1) &&
		(client_data->sig_flag != 1)) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_SUSPEND]);
		bmi_delay(3);
	}
	if (client_data->pw.gyro_pm != BMI_GYRO_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_SUSPEND]);
		bmi_delay(3);
	}

	if (client_data->pw.mag_pm != BMI_MAG_PM_SUSPEND) {
#if defined(BMI160_AKM09912_SUPPORT)
		err += bmi160_set_bst_akm_and_secondary_if_powermode(
		BMI160_MAG_SUSPEND_MODE);
#else
		err += bmi160_set_bmm150_mag_and_secondary_if_power_mode(
		BMI160_MAG_SUSPEND_MODE);
#endif
		bmi_delay(3);
	}

	return err;
}
EXPORT_SYMBOL(bmi_suspend);

int bmi_resume(struct device *dev)
{
	int err = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_client_data *client_data = iio_priv(indio_dev);

	if (client_data->pw.acc_pm != BMI_ACC_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_NORMAL]);
		bmi_delay(3);
	}
	if (client_data->pw.gyro_pm != BMI_GYRO_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_NORMAL]);
		bmi_delay(3);
	}
	if (client_data->pw.mag_pm != BMI_MAG_PM_SUSPEND) {
#if defined(BMI160_AKM09912_SUPPORT)
		err += bmi160_set_bst_akm_and_secondary_if_powermode
					(BMI160_MAG_FORCE_MODE);
#else
		err += bmi160_set_bmm150_mag_and_secondary_if_power_mode
					(BMI160_MAG_FORCE_MODE);
#endif
		bmi_delay(3);
	}

	return err;
}
EXPORT_SYMBOL(bmi_resume);
