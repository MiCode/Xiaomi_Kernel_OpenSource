/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160_driver.c
 * @date     2016/08/01 14:40
 * @id       "b5ff23a"
 * @version  1.3
 *
 * @brief
 * The core code of BMI160 device driver
 *
 * @detail
 * This file implements the core code of BMI160 device driver,
 * which includes hardware related functions, input device register,
 * device attribute files, etc.
*/

#include "bmi160.h"
#include "bmi160_driver.h"
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>


#define DRIVER_VERSION "0.0.53.0"
#define I2C_BURST_READ_MAX_LEN      (256)
#define BMI160_STORE_COUNT  (6000)
#define LMADA     (1)
uint64_t g_current_apts_us;


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

/*! bmi160 acc/gyro calibration status in H/W layer */
enum BMI_CALIBRATION_STATUS_TYPE {
	/*BMI FAST Calibration ready x/y/z status*/
	BMI_ACC_X_FAST_CALI_RDY = 0,
	BMI_ACC_Y_FAST_CALI_RDY,
	BMI_ACC_Z_FAST_CALI_RDY
};

unsigned int reg_op_addr;

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
	BMI160_DATA_RDY_ENABLE,
	BMI160_FIFO_FULL_ENABLE,
	BMI160_FIFO_WM_ENABLE,
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

struct bmi160_store_info_t {
	uint8_t current_frm_cnt;
	uint64_t current_apts_us[2];
	uint8_t fifo_ts_total_frmcnt;
	uint64_t fifo_time;
};

uint64_t get_current_timestamp(void)
{
	uint64_t ts_ap;
	struct timespec tmp_time;
	get_monotonic_boottime(&tmp_time);
	ts_ap = (uint64_t)tmp_time.tv_sec * 1000000000 + tmp_time.tv_nsec;
	return ts_ap;

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

#if defined(CONFIG_USE_QUALCOMM_HAL)
#define POLL_INTERVAL_MIN_MS	10
#define POLL_INTERVAL_MAX_MS	4000
#define POLL_DEFAULT_INTERVAL_MS 200
#define BMI160_ACCEL_MIN_VALUE	-32768
#define BMI160_ACCEL_MAX_VALUE	32767
#define BMI160_GYRO_MIN_VALUE	-32768
#define BMI160_GYRO_MAX_VALUE	32767
#define BMI160_ACCEL_DEFAULT_POLL_INTERVAL_MS	200
#define BMI160_GYRO_DEFAULT_POLL_INTERVAL_MS	200
#define BMI160_ACCEL_MIN_POLL_INTERVAL_MS	10
#define BMI160_ACCEL_MAX_POLL_INTERVAL_MS	5000
#define BMI160_GYRO_MIN_POLL_INTERVAL_MS	10
#define BMI160_GYRO_MAX_POLL_INTERVAL_MS	5000
static struct sensors_classdev bmi160_accel_cdev = {
		.name = "bmi160-accel",
		.vendor = "bosch",
		.version = 1,
		.handle = SENSORS_ACCELERATION_HANDLE,
		.type = SENSOR_TYPE_ACCELEROMETER,
		.max_range = "156.8",	/* 16g */
		.resolution = "0.153125",	/* 15.6mg */
		.sensor_power = "0.13",	/* typical value */
		.min_delay = POLL_INTERVAL_MIN_MS * 1000, /* in microseconds */
		.max_delay = POLL_INTERVAL_MAX_MS,
		.delay_msec = POLL_DEFAULT_INTERVAL_MS, /* in millisecond */
		.fifo_reserved_event_count = 0,
		.fifo_max_event_count = 0,
		.enabled = 0,
		.max_latency = 0,
		.flags = 0,
		.sensors_enable = NULL,
		.sensors_poll_delay = NULL,
		.sensors_set_latency = NULL,
		.sensors_flush = NULL,
		.sensors_self_test = NULL,
};
static struct sensors_classdev bmi160_gyro_cdev = {
	.name = "bmi160-gyro",
	.vendor = "bosch",
	.version = 1,
	.handle = SENSORS_GYROSCOPE_HANDLE,
	.type = SENSOR_TYPE_GYROSCOPE,
	.max_range = "34.906586",	/* rad/s */
	.resolution = "0.0010681152",	/* rad/s */
	.sensor_power = "3.6",	/* 3.6 mA */
	.min_delay = BMI160_GYRO_MIN_POLL_INTERVAL_MS * 1000,
	.max_delay = BMI160_GYRO_MAX_POLL_INTERVAL_MS,
	.delay_msec = BMI160_GYRO_DEFAULT_POLL_INTERVAL_MS,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.max_latency = 0,
	.flags = 0, /* SENSOR_FLAG_CONTINUOUS_MODE */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
	.sensors_enable_wakeup = NULL,
	.sensors_set_latency = NULL,
	.sensors_flush = NULL,
};
#endif
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


static int bmi_input_init(struct bmi_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;
#if defined(CONFIG_USE_QUALCOMM_HAL)
	dev->name = "bmi160-accel";
#else
	dev->name = SENSOR_NAME;
#endif
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_MSC, MSC_GESTURE);
	input_set_capability(dev, EV_MSC, INPUT_EVENT_SGM);

	input_set_capability(dev, EV_MSC, INPUT_EVENT_FAST_GYRO_CALIB_DONE);
	input_set_capability(dev, EV_MSC, INPUT_EVENT_STEP_DETECTOR);
	input_set_capability(dev, EV_MSC, INPUT_EVENT_FAST_ACC_CALIB_DONE);


	input_set_capability(dev, EV_REL, REL_X);
	input_set_capability(dev, EV_REL, REL_Y);
	input_set_capability(dev, EV_REL, REL_Z);
	#if defined(CONFIG_USE_QUALCOMM_HAL)
	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X,
	BMI160_ACCEL_MIN_VALUE, BMI160_ACCEL_MAX_VALUE,
	0, 0);
	input_set_abs_params(dev, ABS_Y,
	BMI160_ACCEL_MIN_VALUE, BMI160_ACCEL_MAX_VALUE,
	0, 0);
	input_set_abs_params(dev, ABS_Z,
	BMI160_ACCEL_MIN_VALUE, BMI160_ACCEL_MAX_VALUE,
	0, 0);
	#endif
	input_set_drvdata(dev, client_data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		dev_notice(client_data->dev, "bmi160 input free!\n");
		return err;
	}
	client_data->input = dev;
	dev_notice(client_data->dev,
		"bmi160 input register successfully, %s!\n",
		client_data->input->name);
	return err;
}

//#if defined(CONFIG_USE_QUALCOMM_HAL)
static int bmi_gyro_input_init(struct bmi_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;
	dev->name = "bmi160-gyro";
	dev->id.bustype = BUS_I2C;
	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_capability(dev, EV_MSC, MSC_GESTURE);
	input_set_capability(dev, EV_MSC, INPUT_EVENT_SGM);
	
	input_set_capability(dev, EV_MSC, INPUT_EVENT_FAST_GYRO_CALIB_DONE);
	input_set_capability(dev, EV_MSC, INPUT_EVENT_STEP_DETECTOR);
	input_set_capability(dev, EV_MSC, INPUT_EVENT_FAST_ACC_CALIB_DONE);
	#if defined(CONFIG_USE_QUALCOMM_HAL)
	input_set_abs_params(dev, ABS_RX,
	BMI160_ACCEL_MIN_VALUE, BMI160_ACCEL_MAX_VALUE,
	0, 0);
	input_set_abs_params(dev, ABS_RY,
	BMI160_ACCEL_MIN_VALUE, BMI160_ACCEL_MAX_VALUE,
	0, 0);
	input_set_abs_params(dev, ABS_RZ,
	BMI160_ACCEL_MIN_VALUE, BMI160_ACCEL_MAX_VALUE,
	0, 0);
	#endif
	input_set_drvdata(dev, client_data);
	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		dev_notice(client_data->dev, "bmi160 input free!\n");
		return err;
	}
	client_data->gyro_input = dev;
	dev_notice(client_data->dev,
		"bmi160 input register successfully, %s!\n",
		client_data->gyro_input->name);
	return err;
}
//#endif
static void bmi_input_destroy(struct bmi_client_data *client_data)
{
	struct input_dev *dev = client_data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int bmi_check_chip_id(struct bmi_client_data *client_data)
{
	int8_t err = 0;
	int8_t i = 0;
	uint8_t chip_id = 0;
	uint8_t read_count = 0;
	u8 bmi_sensor_cnt = sizeof(sensor_type_map)
				/ sizeof(struct bmi160_type_mapping_type);
	/* read and check chip id */
	while (read_count++ < CHECK_CHIP_ID_TIME_MAX) {
		if (client_data->device.bus_read(client_data->device.dev_addr,
				BMI_REG_NAME(USER_CHIP_ID), &chip_id, 1) < 0) {

			dev_err(client_data->dev,
					"Bosch Sensortec Device not found"
						"read chip_id:%d\n", chip_id);
			continue;
		} else {
			for (i = 0; i < bmi_sensor_cnt; i++) {
				if (sensor_type_map[i].chip_id == chip_id) {
					client_data->chip_id = chip_id;
					dev_notice(client_data->dev,
					"Bosch Sensortec Device detected, "
			"HW IC name: %s\n", sensor_type_map[i].sensor_name);
					return 0;
				}
			}
			if (i < bmi_sensor_cnt)
				break;
			else {
				if (read_count == CHECK_CHIP_ID_TIME_MAX) {
					dev_err(client_data->dev,
				"Failed!Bosch Sensortec Device not found"
					" mismatch chip_id:%d\n", chip_id);
					err = -ENODEV;
					return err;
				}
			}
			bmi_delay(1);
		}
	}
	err = -ENODEV;
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

static void bmi_work_func(struct work_struct *work)
{
	struct bmi_client_data *client_data =
		container_of((struct delayed_work *)work,
			struct bmi_client_data, work);
	unsigned long delay =
		msecs_to_jiffies(atomic_read(&client_data->delay));
	struct bmi160_accel_t data;
	int err;

	err = BMI_CALL_API(read_accel_xyz)(&data);
	if (err < 0)
		return;

	/*report current frame via input event*/
	input_event(client_data->input, EV_REL, REL_X, data.x);
	input_event(client_data->input, EV_REL, REL_Y, data.y);
	input_event(client_data->input, EV_REL, REL_Z, data.z);
	input_sync(client_data->input);

	schedule_delayed_work(&client_data->work, delay);
}

static ssize_t bmi160_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "0x%x\n", client_data->chip_id);
}

static ssize_t bmi160_err_st_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	err = bmi_get_err_status(client_data);
	if (err)
		return err;
	else {
		return snprintf(buf, 128, "fatal_err:0x%x, err_code:%d,\n\n"
			"i2c_fail_err:%d, drop_cmd_err:%d, mag_drdy_err:%d\n",
			client_data->err_st.fatal_err,
			client_data->err_st.err_code,
			client_data->err_st.i2c_fail,
			client_data->err_st.drop_cmd,
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
		client_data->fifo_data_sel = fifo_datasel;
	}
	return count;
}

static ssize_t bmi160_fifo_data_out_frame_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	int err = 0;
	uint32_t fifo_bytecount = 0;

	err = BMI_CALL_API(fifo_length)(&fifo_bytecount);
	if (err < 0) {
		dev_err(client_data->dev, "read fifo_length err");
		return -EINVAL;
	}
	if (fifo_bytecount == 0)
		return 0;
	err = bmi_burst_read_wrapper(client_data->device.dev_addr,
		BMI160_USER_FIFO_DATA__REG, buf,
		fifo_bytecount);
	if (err) {
		dev_err(client_data->dev, "read fifo err");
		BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);
		return -EINVAL;
	}
	return fifo_bytecount;

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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

static ssize_t bmi160_place_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

	if (NULL != client_data->bst_pd)
		place = client_data->bst_pd->place;

	return snprintf(buf, 16, "%d\n", place);
}

static ssize_t bmi160_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "%d\n", atomic_read(&client_data->delay));

}

static ssize_t bmi160_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	if (data == 0) {
		err = -EINVAL;
		return err;
	}

	if (data < BMI_DELAY_MIN)
		data = BMI_DELAY_MIN;

	atomic_set(&client_data->delay, (unsigned int)data);

	return count;
}

static ssize_t bmi160_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "%d\n", atomic_read(&client_data->wkqueue_en));

}

static ssize_t bmi160_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long enable;
	int pre_enable = atomic_read(&client_data->wkqueue_en);

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;

	enable = enable ? 1 : 0;
	mutex_lock(&client_data->mutex_enable);
	if (enable) {
		if (pre_enable == 0) {
			bmi160_set_acc_op_mode(client_data,
							BMI_ACC_PM_NORMAL);
			schedule_delayed_work(&client_data->work,
			msecs_to_jiffies(atomic_read(&client_data->delay)));
			atomic_set(&client_data->wkqueue_en, 1);
		}

	} else {
		if (pre_enable == 1) {
			bmi160_set_acc_op_mode(client_data,
							BMI_ACC_PM_SUSPEND);

			cancel_delayed_work_sync(&client_data->work);
			atomic_set(&client_data->wkqueue_en, 0);
		}
	}

	mutex_unlock(&client_data->mutex_enable);

	return count;
}

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	err = BMI_CALL_API(get_step_detector_enable)(&step_det);
	/*bmi160_get_status0_step_int*/
	if (err < 0)
		return err;
/*client_data->std will be updated in bmi_stepdetector_interrupt_handle */
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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

static ssize_t bmi160_signification_motion_enable_store(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
		enable_irq_wake(client_data->IRQ);
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
		disable_irq_wake(client_data->IRQ);
		client_data->sig_flag = 0;
	}
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = BMI_CALL_API(get_accel_range)(&range);
	if (err)
		return err;

	client_data->range.acc_range = range;
	return snprintf(buf, 16, "%d\n", range);
}

static ssize_t bmi160_acc_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);


	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_range)(range);
	if (err)
		return -EIO;

	client_data->range.acc_range = range;
	return count;
}

static ssize_t bmi160_acc_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char acc_odr;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long op_mode;
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	else
		client_data->calib_status |=
			BMI_FAST_CALI_TRUE << BMI_ACC_X_FAST_CALI_RDY;
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	else
		client_data->calib_status |=
			BMI_FAST_CALI_TRUE << BMI_ACC_Y_FAST_CALI_RDY;
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	unsigned char data1[3] = {0};
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
	else
		client_data->calib_status |=
			BMI_FAST_CALI_TRUE << BMI_ACC_Z_FAST_CALI_RDY;

	if (client_data->calib_status == BMI_FAST_CALI_ALL_RDY) {
		err = BMI_CALL_API(get_accel_offset_compensation_xaxis)(
			&data1[0]);
		err += BMI_CALL_API(get_accel_offset_compensation_yaxis)(
			&data1[1]);
		err += BMI_CALL_API(get_accel_offset_compensation_zaxis)(
			&data1[2]);
		dev_info(client_data->dev, "accx %d, accy %d, accz %d\n",
			data1[0], data1[1], data1[2]);
		if (err)
			return -EIO;
		input_event(client_data->input, EV_MSC,
		INPUT_EVENT_FAST_ACC_CALIB_DONE,
		(data1[0] | (data1[1] << 8) | (data1[2] << 16)));
		input_sync(client_data->input);
		client_data->calib_status = 0;
	}

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	u16 data;
	int err;
	static u16 last_stc_value;

	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "0x%x\n",
				atomic_read(&client_data->selftest_result));
}

static int bmi_restore_hw_cfg(struct bmi_client_data *client);

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	int i = 0;

	u8 acc_selftest = 0;
	u8 gyro_selftest = 0;
	u8 bmi_selftest = 0;
	s16 axis_p_value, axis_n_value;
	u16 diff_axis[3] = {0xff, 0xff, 0xff};
	u8 acc_odr, range, acc_selftest_amp, acc_selftest_sign;

	dev_notice(client_data->dev, "Selftest for BMI16x starting.\n");

	client_data->selftest = 1;

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
			client_data->selftest = 0;
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
	if (err) {
		client_data->selftest = 0;
		return err;
	}
	msleep(50);

	bmi_restore_hw_cfg(client_data);

	client_data->selftest = 0;
	dev_notice(client_data->dev, "Selftest for BMI16x finished\n");

	return count;
}

/* gyro sensor part */
static ssize_t bmi160_gyro_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	unsigned long op_mode;
	int err;

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = BMI_CALL_API(get_gyro_range)(&range);
	if (err)
		return err;

	client_data->range.gyro_range = range;
	return snprintf(buf, 16, "%d\n", range);
}

static ssize_t bmi160_gyro_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_range)(range);
	if (err)
		return -EIO;

	client_data->range.gyro_range = range;
	return count;
}

static ssize_t bmi160_gyro_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char gyro_odr;
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;

	err = BMI_CALL_API(set_foc_gyro_enable)((u8)enable,
				&gyr_off_x, &gyr_off_y, &gyr_off_z);

	if (err < 0)
		return -EIO;
	else {
		input_event(client_data->input, EV_MSC,
			INPUT_EVENT_FAST_GYRO_CALIB_DONE, 1);
		input_sync(client_data->input);
	}
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


/* mag sensor part */
#ifdef BMI160_MAG_INTERFACE_SUPPORT
static ssize_t bmi160_mag_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &mag_odr);
	if (err)
		return err;
	/*1~25/32hz,..6(25hz),7(50hz),... */
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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

	return snprintf(buf, 16, "%x\n", mag_chipid);

}

static ssize_t bmi160_mag_chip_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 mag_chipid = 0;
#if defined(BMI160_AKM09912_SUPPORT)
	mag_chipid = 15;
#else
	mag_chipid = 150;
#endif
	return snprintf(buf, 16, "%d\n", mag_chipid);
}

struct bmi160_mag_xyz_s32_t mag_compensate;
static ssize_t bmi160_mag_compensate_xyz_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	memcpy(buf, &mag_compensate, sizeof(mag_compensate));
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);

	ssize_t ret;
	u8 reg_data[128], i;
	int pos;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	ret = bmi_burst_read_wrapper(client_data->device.dev_addr,
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
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

	ret = BMI_CALL_API(write_reg)(
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
	struct input_dev *input = to_input_dev(dev);
	struct bmi_client_data *client_data = input_get_drvdata(input);
	int ret;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	ret = snprintf(buf, 128, "Driver version: %s\n",
			DRIVER_VERSION);

	return ret;
}
static DEVICE_ATTR(chip_id, S_IRUGO,
		bmi160_chip_id_show, NULL);
static DEVICE_ATTR(err_st, S_IRUGO,
		bmi160_err_st_show, NULL);
static DEVICE_ATTR(sensor_time, S_IRUGO,
		bmi160_sensor_time_show, NULL);

static DEVICE_ATTR(selftest, S_IRUGO | S_IWUSR,
		bmi160_selftest_show, bmi160_selftest_store);
static DEVICE_ATTR(fifo_flush, S_IRUGO | S_IWUSR,
		NULL, bmi160_fifo_flush_store);
static DEVICE_ATTR(fifo_bytecount, S_IRUGO | S_IWUSR,
		bmi160_fifo_bytecount_show, bmi160_fifo_bytecount_store);
static DEVICE_ATTR(fifo_data_sel, S_IRUGO | S_IWUSR,
		bmi160_fifo_data_sel_show, bmi160_fifo_data_sel_store);
static DEVICE_ATTR(fifo_data_frame, S_IRUGO,
		bmi160_fifo_data_out_frame_show, NULL);

static DEVICE_ATTR(fifo_watermark, S_IRUGO | S_IWUSR,
		bmi160_fifo_watermark_show, bmi160_fifo_watermark_store);

static DEVICE_ATTR(fifo_header_en, S_IRUGO | S_IWUSR,
		bmi160_fifo_header_en_show, bmi160_fifo_header_en_store);
static DEVICE_ATTR(fifo_time_en, S_IRUGO | S_IWUSR,
		bmi160_fifo_time_en_show, bmi160_fifo_time_en_store);
static DEVICE_ATTR(fifo_int_tag_en, S_IRUGO | S_IWUSR,
		bmi160_fifo_int_tag_en_show, bmi160_fifo_int_tag_en_store);

static DEVICE_ATTR(temperature, S_IRUGO,
		bmi160_temperature_show, NULL);
static DEVICE_ATTR(place, S_IRUGO,
		bmi160_place_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO | S_IWUSR,
		bmi160_delay_show, bmi160_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR,
		bmi160_enable_show, bmi160_enable_store);
static DEVICE_ATTR(acc_range, S_IRUGO | S_IWUSR,
		bmi160_acc_range_show, bmi160_acc_range_store);
static DEVICE_ATTR(acc_odr, S_IRUGO | S_IWUSR,
		bmi160_acc_odr_show, bmi160_acc_odr_store);
static DEVICE_ATTR(acc_op_mode, S_IRUGO | S_IWUSR,
		bmi160_acc_op_mode_show, bmi160_acc_op_mode_store);
static DEVICE_ATTR(acc_value, S_IRUGO,
		bmi160_acc_value_show, NULL);
static DEVICE_ATTR(acc_fast_calibration_x, S_IRUGO | S_IWUSR,
		bmi160_acc_fast_calibration_x_show,
		bmi160_acc_fast_calibration_x_store);
static DEVICE_ATTR(acc_fast_calibration_y, S_IRUGO | S_IWUSR,
		bmi160_acc_fast_calibration_y_show,
		bmi160_acc_fast_calibration_y_store);
static DEVICE_ATTR(acc_fast_calibration_z, S_IRUGO | S_IWUSR,
		bmi160_acc_fast_calibration_z_show,
		bmi160_acc_fast_calibration_z_store);
static DEVICE_ATTR(acc_offset_x, S_IRUGO | S_IWUSR,
		bmi160_acc_offset_x_show,
		bmi160_acc_offset_x_store);
static DEVICE_ATTR(acc_offset_y, S_IRUGO | S_IWUSR,
		bmi160_acc_offset_y_show,
		bmi160_acc_offset_y_store);
static DEVICE_ATTR(acc_offset_z, S_IRUGO | S_IWUSR,
		bmi160_acc_offset_z_show,
		bmi160_acc_offset_z_store);
static DEVICE_ATTR(test, S_IRUGO,
		bmi160_test_show, NULL);
static DEVICE_ATTR(stc_enable, S_IRUGO | S_IWUSR,
		bmi160_step_counter_enable_show,
		bmi160_step_counter_enable_store);
static DEVICE_ATTR(stc_mode, S_IRUGO | S_IWUSR,
		NULL, bmi160_step_counter_mode_store);
static DEVICE_ATTR(stc_clc, S_IRUGO | S_IWUSR,
		NULL, bmi160_step_counter_clc_store);
static DEVICE_ATTR(stc_value, S_IRUGO,
		bmi160_step_counter_value_show, NULL);
static DEVICE_ATTR(reg_sel, S_IRUGO | S_IWUSR,
		bmi160_show_reg_sel, bmi160_store_reg_sel);
static DEVICE_ATTR(reg_val, S_IRUGO | S_IWUSR,
		bmi160_show_reg_val, bmi160_store_reg_val);
static DEVICE_ATTR(driver_version, S_IRUGO,
		bmi160_driver_version_show, NULL);
/* gyro part */
static DEVICE_ATTR(gyro_op_mode, S_IRUGO | S_IWUSR,
		bmi160_gyro_op_mode_show, bmi160_gyro_op_mode_store);
static DEVICE_ATTR(gyro_value, S_IRUGO,
		bmi160_gyro_value_show, NULL);
static DEVICE_ATTR(gyro_range, S_IRUGO | S_IWUSR,
		bmi160_gyro_range_show, bmi160_gyro_range_store);
static DEVICE_ATTR(gyro_odr, S_IRUGO | S_IWUSR,
		bmi160_gyro_odr_show, bmi160_gyro_odr_store);
static DEVICE_ATTR(gyro_fast_calibration_en, S_IRUGO | S_IWUSR,
bmi160_gyro_fast_calibration_en_show, bmi160_gyro_fast_calibration_en_store);
static DEVICE_ATTR(gyro_offset_x, S_IRUGO | S_IWUSR,
bmi160_gyro_offset_x_show, bmi160_gyro_offset_x_store);
static DEVICE_ATTR(gyro_offset_y, S_IRUGO | S_IWUSR,
bmi160_gyro_offset_y_show, bmi160_gyro_offset_y_store);
static DEVICE_ATTR(gyro_offset_z, S_IRUGO | S_IWUSR,
bmi160_gyro_offset_z_show, bmi160_gyro_offset_z_store);

#ifdef BMI160_MAG_INTERFACE_SUPPORT
static DEVICE_ATTR(mag_op_mode, S_IRUGO | S_IWUSR,
		bmi160_mag_op_mode_show, bmi160_mag_op_mode_store);
static DEVICE_ATTR(mag_odr, S_IRUGO | S_IWUSR,
		bmi160_mag_odr_show, bmi160_mag_odr_store);
static DEVICE_ATTR(mag_i2c_addr, S_IRUGO | S_IWUSR,
		bmi160_mag_i2c_address_show, bmi160_mag_i2c_address_store);
static DEVICE_ATTR(mag_value, S_IRUGO,
		bmi160_mag_value_show, NULL);
static DEVICE_ATTR(mag_offset, S_IRUGO | S_IWUSR,
		bmi160_mag_offset_show, bmi160_mag_offset_store);
static DEVICE_ATTR(mag_chip_id, S_IRUGO,
		bmi160_mag_chip_id_show, NULL);
static DEVICE_ATTR(mag_chip_name, S_IRUGO,
		bmi160_mag_chip_name_show, NULL);
static DEVICE_ATTR(mag_compensate, S_IRUGO | S_IWUSR,
		bmi160_mag_compensate_xyz_show,
		bmi160_mag_compensate_xyz_store);
#endif


#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static DEVICE_ATTR(enable_int, S_IRUGO | S_IWUSR,
		NULL, bmi_enable_int_store);
static DEVICE_ATTR(anymot_duration, S_IRUGO | S_IWUSR,
		bmi160_anymot_duration_show, bmi160_anymot_duration_store);
static DEVICE_ATTR(anymot_threshold, S_IRUGO | S_IWUSR,
		bmi160_anymot_threshold_show, bmi160_anymot_threshold_store);
static DEVICE_ATTR(std_stu, S_IRUGO,
		bmi160_step_detector_status_show, NULL);
static DEVICE_ATTR(std_en, S_IRUGO | S_IWUSR,
		bmi160_step_detector_enable_show,
		bmi160_step_detector_enable_store);
static DEVICE_ATTR(sig_en, S_IRUGO | S_IWUSR,
		bmi160_signification_motion_enable_show,
		bmi160_signification_motion_enable_store);

#endif



static DEVICE_ATTR(bmi_value, S_IRUGO,
		bmi160_bmi_value_show, NULL);


static struct attribute *bmi160_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_err_st.attr,
	&dev_attr_sensor_time.attr,
	&dev_attr_selftest.attr,
	&dev_attr_driver_version.attr,
	&dev_attr_test.attr,
	&dev_attr_fifo_flush.attr,
	&dev_attr_fifo_header_en.attr,
	&dev_attr_fifo_time_en.attr,
	&dev_attr_fifo_int_tag_en.attr,
	&dev_attr_fifo_bytecount.attr,
	&dev_attr_fifo_data_sel.attr,
	&dev_attr_fifo_data_frame.attr,

	&dev_attr_fifo_watermark.attr,

	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_temperature.attr,
	&dev_attr_place.attr,

	&dev_attr_acc_range.attr,
	&dev_attr_acc_odr.attr,
	&dev_attr_acc_op_mode.attr,
	&dev_attr_acc_value.attr,

	&dev_attr_acc_fast_calibration_x.attr,
	&dev_attr_acc_fast_calibration_y.attr,
	&dev_attr_acc_fast_calibration_z.attr,
	&dev_attr_acc_offset_x.attr,
	&dev_attr_acc_offset_y.attr,
	&dev_attr_acc_offset_z.attr,

	&dev_attr_stc_enable.attr,
	&dev_attr_stc_mode.attr,
	&dev_attr_stc_clc.attr,
	&dev_attr_stc_value.attr,

	&dev_attr_gyro_op_mode.attr,
	&dev_attr_gyro_value.attr,
	&dev_attr_gyro_range.attr,
	&dev_attr_gyro_odr.attr,
	&dev_attr_gyro_fast_calibration_en.attr,
	&dev_attr_gyro_offset_x.attr,
	&dev_attr_gyro_offset_y.attr,
	&dev_attr_gyro_offset_z.attr,

#ifdef BMI160_MAG_INTERFACE_SUPPORT
	&dev_attr_mag_chip_id.attr,
	&dev_attr_mag_op_mode.attr,
	&dev_attr_mag_odr.attr,
	&dev_attr_mag_i2c_addr.attr,
	&dev_attr_mag_chip_name.attr,
	&dev_attr_mag_value.attr,
	&dev_attr_mag_offset.attr,
	&dev_attr_mag_compensate.attr,
#endif

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
	&dev_attr_enable_int.attr,

	&dev_attr_anymot_duration.attr,
	&dev_attr_anymot_threshold.attr,
	&dev_attr_std_stu.attr,
	&dev_attr_std_en.attr,
	&dev_attr_sig_en.attr,

#endif
	&dev_attr_reg_sel.attr,
	&dev_attr_reg_val.attr,
	&dev_attr_bmi_value.attr,
	NULL
};

static struct attribute_group bmi160_attribute_group = {
	.attrs = bmi160_attributes
};

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static void bmi_slope_interrupt_handle(struct bmi_client_data *client_data)
{
	/* anym_first[0..2]: x, y, z */
	u8 anym_first[3] = {0};
	u8 status2;
	u8 anym_sign;
	u8 i = 0;

	client_data->device.bus_read(client_data->device.dev_addr,
				BMI160_USER_INTR_STAT_2_ADDR, &status2, 1);
	anym_first[0] = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_X);
	anym_first[1] = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_Y);
	anym_first[2] = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_Z);
	anym_sign = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INTR_STAT_2_ANY_MOTION_SIGN);

	for (i = 0; i < 3; i++) {
		if (anym_first[i]) {
			/*1: negative*/
			if (anym_sign)
				dev_notice(client_data->dev,
				"Anymotion interrupt happend!"
				"%s axis, negative sign\n", bmi_axis_name[i]);
			else
				dev_notice(client_data->dev,
				"Anymotion interrupt happend!"
				"%s axis, postive sign\n", bmi_axis_name[i]);
		}
	}


}

static void bmi_fifo_watermark_interrupt_handle
				(struct bmi_client_data *client_data)
{
	int err = 0;
	unsigned int fifo_len0 = 0;
	unsigned int  fifo_frmbytes_ext = 0;
	unsigned char *fifo_data = NULL;
	fifo_data = kzalloc(FIFO_DATA_BUFSIZE, GFP_KERNEL);
	/*TO DO*/
	if (NULL == fifo_data) {
			dev_err(client_data->dev, "no memory available");
			err = -ENOMEM;
	}
	bmi_fifo_frame_bytes_extend_calc(client_data, &fifo_frmbytes_ext);

	if (client_data->pw.acc_pm == 2 && client_data->pw.gyro_pm == 2
					&& client_data->pw.mag_pm == 2)
		printk(KERN_INFO "pw_acc: %d, pw_gyro: %d\n",
			client_data->pw.acc_pm, client_data->pw.gyro_pm);
	if (!client_data->fifo_data_sel)
		printk(KERN_INFO "no selsect sensor fifo, fifo_data_sel:%d\n",
						client_data->fifo_data_sel);

	err = BMI_CALL_API(fifo_length)(&fifo_len0);
	client_data->fifo_bytecount = fifo_len0;

	if (client_data->fifo_bytecount == 0 || err)
		return;

	if (client_data->fifo_bytecount + fifo_frmbytes_ext > FIFO_DATA_BUFSIZE)
		client_data->fifo_bytecount = FIFO_DATA_BUFSIZE;
	/* need give attention for the time of burst read*/
	if (!err) {
		err = bmi_burst_read_wrapper(client_data->device.dev_addr,
			BMI160_USER_FIFO_DATA__REG, fifo_data,
			client_data->fifo_bytecount + fifo_frmbytes_ext);
	} else
		dev_err(client_data->dev, "read fifo leght err");

	if (err)
		dev_err(client_data->dev, "brust read fifo err\n");
	/*err = bmi_fifo_analysis_handle(client_data, fifo_data,
			client_data->fifo_bytecount + 20, fifo_out_data);*/
	if (fifo_data != NULL) {
		kfree(fifo_data);
		fifo_data = NULL;
	}

}
static void bmi_data_ready_interrupt_handle(
	struct bmi_client_data *client_data, uint8_t status)
{
	uint8_t data12[12] = {0};
	struct bmi160_accel_t accel;
	struct bmi160_gyro_t gyro;
	struct timespec ts;
	client_data->device.bus_read(client_data->device.dev_addr,
	BMI160_USER_DATA_8_ADDR, data12, 12);
	if (status & 0x80)
	{
		/*report acc data*/
		/* Data X */
		accel.x = (s16)((((s32)((s8)data12[7])) << BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[6]));
		/* Data Y */
		accel.y = (s16)((((s32)((s8)data12[9])) << BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[8]));
		/* Data Z */
		accel.z = (s16)((((s32)((s8)data12[11]))<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[10]));
		ts = ns_to_timespec(client_data->timestamp);
		//printk("acc puneet ts.tv_sec %ld ts.tv_nsec %ld\n",ts.tv_sec,ts.tv_nsec);
		input_event(client_data->input, EV_MSC, 6, ts.tv_sec);
		input_event(client_data->input, EV_MSC, 6, ts.tv_nsec);
		input_event(client_data->input, EV_MSC, MSC_GESTURE, accel.x);
		input_event(client_data->input, EV_MSC, MSC_RAW, accel.y);
		input_event(client_data->input, EV_MSC, MSC_SCAN, accel.z);
		input_sync(client_data->input);
	}
	if (status & 0x40)
	{
		/*report gyro data*/
		/* Data X */
		gyro.x = (s16)((((s32)((s8)data12[1])) << BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[0]));
		/* Data Y */
		gyro.y = (s16)((((s32)((s8)data12[3])) << BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[2]));
		/* Data Z */
		gyro.z = (s16)((((s32)((s8)data12[5]))<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[4]));
		ts = ns_to_timespec(client_data->timestamp);
		//printk("gyro puneet ts.tv_sec %ld ts.tv_nsec %ld\n",ts.tv_sec,ts.tv_nsec);
		input_event(client_data->gyro_input, EV_MSC, 6, ts.tv_sec);
		input_event(client_data->gyro_input, EV_MSC, 6, ts.tv_nsec);
		input_event(client_data->gyro_input, EV_MSC, MSC_GESTURE, gyro.x);
		input_event(client_data->gyro_input, EV_MSC, MSC_RAW, gyro.y);
		input_event(client_data->gyro_input, EV_MSC, MSC_SCAN, gyro.z);
		input_sync(client_data->gyro_input);
	}
}

static void bmi_signification_motion_interrupt_handle(
		struct bmi_client_data *client_data)
{
	printk(KERN_INFO "bmi_signification_motion_interrupt_handle\n");
	input_event(client_data->input, EV_MSC, INPUT_EVENT_SGM, 1);
/*input_report_rel(client_data->input,INPUT_EVENT_SGM,1);*/
	input_sync(client_data->input);
	bmi160_set_command_register(CMD_RESET_INT_ENGINE);

}
static void bmi_stepdetector_interrupt_handle(
	struct bmi_client_data *client_data)
{
	u8 current_step_dector_st = 0;
	client_data->pedo_data.wkar_step_detector_status++;
	current_step_dector_st =
		client_data->pedo_data.wkar_step_detector_status;
	client_data->std = ((current_step_dector_st == 1) ? 0 : 1);

	input_event(client_data->input, EV_MSC, INPUT_EVENT_STEP_DETECTOR, 1);
	input_sync(client_data->input);
}

static void bmi_irq_work_func(struct work_struct *work)
{
	struct bmi_client_data *client_data =
		container_of((struct work_struct *)work,
			struct bmi_client_data, irq_work);

	unsigned char int_status[4] = {0, 0, 0, 0};
	uint8_t status = 0;

	//client_data->device.bus_read(client_data->device.dev_addr,
	//			BMI160_USER_INTR_STAT_0_ADDR, int_status, 4);
	client_data->device.bus_read(client_data->device.dev_addr,
	BMI160_USER_STAT_ADDR, &status, 1);
	//printk("status = 0x%x", status);
	if (BMI160_GET_BITSLICE(int_status[0],
					BMI160_USER_INTR_STAT_0_ANY_MOTION))
		bmi_slope_interrupt_handle(client_data);

	if (BMI160_GET_BITSLICE(int_status[0],
			BMI160_USER_INTR_STAT_0_STEP_INTR))
		bmi_stepdetector_interrupt_handle(client_data);
	if (BMI160_GET_BITSLICE(int_status[1],
			BMI160_USER_INTR_STAT_1_FIFO_WM_INTR))
		bmi_fifo_watermark_interrupt_handle(client_data);
	if ((status & 0x80) || (status & 0x40))
		bmi_data_ready_interrupt_handle(client_data, status);
	/* Clear ALL inputerrupt status after handler sig mition*/
	/* Put this commads intot the last one*/
	if (BMI160_GET_BITSLICE(int_status[0],
		BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR))
		bmi_signification_motion_interrupt_handle(client_data);

}

static void bmi160_delay_sigmo_work_func(struct work_struct *work)
{
	struct bmi_client_data *client_data =
	container_of(work, struct bmi_client_data,
	delay_work_sig.work);
	unsigned char int_status[4] = {0, 0, 0, 0};

	client_data->device.bus_read(client_data->device.dev_addr,
				BMI160_USER_INTR_STAT_0_ADDR, int_status, 4);
	if (BMI160_GET_BITSLICE(int_status[0],
		BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR))
		bmi_signification_motion_interrupt_handle(client_data);
}

static irqreturn_t bmi_irq_handler(int irq, void *handle)
{
	struct bmi_client_data *client_data = handle;
	int in_suspend_copy;
	in_suspend_copy = atomic_read(&client_data->in_suspend);
	client_data->timestamp= get_current_timestamp();
	if (client_data == NULL)
		return IRQ_HANDLED;
	if (client_data->dev == NULL)
		return IRQ_HANDLED;
		/*this only deal with SIG_motion CTS test*/
	if ((in_suspend_copy == 1) &&
		(client_data->sig_flag == 1)) {
		/*wake_lock_timeout(&client_data->wakelock, HZ);*/
		schedule_delayed_work(&client_data->delay_work_sig,
			msecs_to_jiffies(50));
	}
	schedule_work(&client_data->irq_work);

	return IRQ_HANDLED;
}
#endif /* defined(BMI_ENABLE_INT1)||defined(BMI_ENABLE_INT2) */

static int bmi_restore_hw_cfg(struct bmi_client_data *client)
{
	int err = 0;

	if ((client->fifo_data_sel) & (1 << BMI_ACC_SENSOR)) {
		err += BMI_CALL_API(set_accel_range)(client->range.acc_range);
		err += BMI_CALL_API(set_accel_output_data_rate)
				(client->odr.acc_odr);
		err += BMI_CALL_API(set_fifo_accel_enable)(1);
	}
	if ((client->fifo_data_sel) & (1 << BMI_GYRO_SENSOR)) {
		err += BMI_CALL_API(set_gyro_range)(client->range.gyro_range);
		err += BMI_CALL_API(set_gyro_output_data_rate)
				(client->odr.gyro_odr);
		err += BMI_CALL_API(set_fifo_gyro_enable)(1);
	}
	if ((client->fifo_data_sel) & (1 << BMI_MAG_SENSOR)) {
		err += BMI_CALL_API(set_mag_output_data_rate)
				(client->odr.mag_odr);
		err += BMI_CALL_API(set_fifo_mag_enable)(1);
	}
	err += BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);

	mutex_lock(&client->mutex_op_mode);
	if (client->pw.acc_pm != BMI_ACC_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_NORMAL]);
		bmi_delay(3);
	}
	mutex_unlock(&client->mutex_op_mode);

	mutex_lock(&client->mutex_op_mode);
	if (client->pw.gyro_pm != BMI_GYRO_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_NORMAL]);
		bmi_delay(3);
	}
	mutex_unlock(&client->mutex_op_mode);

	mutex_lock(&client->mutex_op_mode);

	if (client->pw.mag_pm != BMI_MAG_PM_SUSPEND) {
#ifdef BMI160_AKM09912_SUPPORT
		err += bmi160_set_bst_akm_and_secondary_if_powermode
					(BMI160_MAG_FORCE_MODE);
#else
		err += bmi160_set_bmm150_mag_and_secondary_if_power_mode
					(BMI160_MAG_FORCE_MODE);
#endif
		bmi_delay(3);
	}
	mutex_unlock(&client->mutex_op_mode);

	return err;
}

#if defined(CONFIG_USE_QUALCOMM_HAL)
static void bmi160_accel_work_fn(struct work_struct *work)
{
	struct bmi_client_data *sensor;
	ktime_t timestamp;
	struct bmi160_accel_t data;
	int err;
	sensor = container_of((struct delayed_work *)work,
				struct bmi_client_data, accel_poll_work);
	timestamp = ktime_get();
	err = BMI_CALL_API(read_accel_xyz)(&data);
	if (err)
		dev_err(sensor->dev, "read data err");
	input_report_abs(sensor->input, ABS_X,
		(data.x));
	input_report_abs(sensor->input, ABS_Y,
		(data.y));
	input_report_abs(sensor->input, ABS_Z,
		(data.z));
	input_event(sensor->input,
			EV_SYN, SYN_TIME_SEC,
			ktime_to_timespec(timestamp).tv_sec);
	input_event(sensor->input, EV_SYN,
		SYN_TIME_NSEC,
		ktime_to_timespec(timestamp).tv_nsec);
	input_sync(sensor->input);
	if (atomic_read(&sensor->accel_en))
		queue_delayed_work(sensor->data_wq,
			&sensor->accel_poll_work,
			msecs_to_jiffies(sensor->accel_poll_ms));
}
static void bmi160_gyro_work_fn(struct work_struct *work)
{
	struct bmi_client_data *sensor;
	ktime_t timestamp;
	struct bmi160_gyro_t data;
	int err;
	sensor = container_of((struct delayed_work *)work,
				struct bmi_client_data, gyro_poll_work);
	timestamp = ktime_get();
	err = BMI_CALL_API(read_gyro_xyz)(&data);
	if (err)
		dev_err(sensor->dev, "read data err");
	input_report_abs(sensor->gyro_input, ABS_RX,
		(data.x));
	input_report_abs(sensor->gyro_input, ABS_RY,
		(data.y));
	input_report_abs(sensor->gyro_input, ABS_RZ,
		(data.z));
	input_event(sensor->gyro_input,
			EV_SYN, SYN_TIME_SEC,
			ktime_to_timespec(timestamp).tv_sec);
	input_event(sensor->gyro_input, EV_SYN,
		SYN_TIME_NSEC,
		ktime_to_timespec(timestamp).tv_nsec);
	input_sync(sensor->gyro_input);
	if (atomic_read(&sensor->gyro_en))
		queue_delayed_work(sensor->data_wq,
			&sensor->gyro_poll_work,
			msecs_to_jiffies(sensor->gyro_poll_ms));
}
static int bmi160_set_gyro_op_mode(struct bmi_client_data *client_data,
							unsigned long op_mode)
{
	int err = 0;
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
	return err;
}
static int bmi160_accel_set_enable(
	struct bmi_client_data *client_data, bool enable)
{
	int ret = 0;
	dev_notice(client_data->dev,
		"bmi160_accel_set_enable enable=%d\n", enable);
	if (enable) {
		ret = bmi160_set_acc_op_mode(client_data, 0);
		if (ret) {
			dev_err(client_data->dev,
				"Fail to enable accel engine ret=%d\n", ret);
			ret = -EBUSY;
			goto exit;
		}
		queue_delayed_work(client_data->data_wq,
				&client_data->accel_poll_work,
				msecs_to_jiffies(client_data->accel_poll_ms));
		atomic_set(&client_data->accel_en, 1);
	} else {
		atomic_set(&client_data->accel_en, 0);
		cancel_delayed_work_sync(&client_data->accel_poll_work);
		ret = bmi160_set_acc_op_mode(client_data, 2);
		if (ret) {
			dev_err(client_data->dev,
				"Fail to disable accel engine ret=%d\n", ret);
			ret = -EBUSY;
			goto exit;
		}
	}
exit:
	return ret;
}
static int bmi160_accel_set_poll_delay(struct bmi_client_data *client_data,
					unsigned long delay)
{
	dev_info(client_data->dev,
		"bmi160_accel_set_poll_delay delay_ms=%ld\n", delay);
	if (delay < BMI160_ACCEL_MIN_POLL_INTERVAL_MS)
		delay = BMI160_ACCEL_MIN_POLL_INTERVAL_MS;
	if (delay > BMI160_ACCEL_MAX_POLL_INTERVAL_MS)
		delay = BMI160_ACCEL_MAX_POLL_INTERVAL_MS;
	client_data->accel_poll_ms = delay;
	if (!atomic_read(&client_data->accel_en))
		goto exit;
	cancel_delayed_work_sync(&client_data->accel_poll_work);
	queue_delayed_work(client_data->data_wq,
			&client_data->accel_poll_work,
			msecs_to_jiffies(client_data->accel_poll_ms));
exit:
	return 0;
}
static int bmi160_gyro_set_enable(
	struct bmi_client_data *client_data, bool enable)
{
	int ret = 0;
	dev_notice(client_data->dev,
		"bmi160_gyro_set_enable enable=%d\n", enable);
	if (enable) {
		ret = bmi160_set_gyro_op_mode(client_data, 0);
		if (ret) {
			dev_err(client_data->dev,
				"Fail to enable gyro engine ret=%d\n", ret);
			ret = -EBUSY;
			goto exit;
		}
		queue_delayed_work(client_data->data_wq,
				&client_data->gyro_poll_work,
				msecs_to_jiffies(client_data->gyro_poll_ms));
		atomic_set(&client_data->gyro_en, 1);
	} else {
		atomic_set(&client_data->gyro_en, 0);
		cancel_delayed_work_sync(&client_data->gyro_poll_work);
		ret = bmi160_set_gyro_op_mode(client_data, 2);
		if (ret) {
			dev_err(client_data->dev,
				"Fail to disable accel engine ret=%d\n", ret);
			ret = -EBUSY;
			goto exit;
		}
	}
exit:
	return ret;
}
static int bmi160_gyro_set_poll_delay(struct bmi_client_data *client_data,
					unsigned long delay)
{
	dev_info(client_data->dev,
		"bmi160_accel_set_poll_delay delay_ms=%ld\n", delay);
	if (delay < BMI160_GYRO_MIN_POLL_INTERVAL_MS)
		delay = BMI160_GYRO_MIN_POLL_INTERVAL_MS;
	if (delay > BMI160_GYRO_MAX_POLL_INTERVAL_MS)
		delay = BMI160_GYRO_MAX_POLL_INTERVAL_MS;
	client_data->gyro_poll_ms = delay;
	if (!atomic_read(&client_data->gyro_en))
		goto exit;
	cancel_delayed_work_sync(&client_data->gyro_poll_work);
	queue_delayed_work(client_data->data_wq,
			&client_data->gyro_poll_work,
			msecs_to_jiffies(client_data->gyro_poll_ms));
exit:
	return 0;
}
static int bmi160_accel_cdev_enable(struct sensors_classdev *sensors_cdev,
			unsigned int enable)
{
	struct bmi_client_data *sensor = container_of(sensors_cdev,
			struct bmi_client_data, accel_cdev);
	return bmi160_accel_set_enable(sensor, enable);
}
static int bmi160_accel_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
			unsigned int delay_ms)
{
	struct bmi_client_data *sensor = container_of(sensors_cdev,
			struct bmi_client_data, accel_cdev);

	return bmi160_accel_set_poll_delay(sensor, delay_ms);
}

static int bmi160_gyro_cdev_enable(struct sensors_classdev *sensors_cdev,
			unsigned int enable)
{
	struct bmi_client_data *sensor = container_of(sensors_cdev,
			struct bmi_client_data, gyro_cdev);

	return bmi160_gyro_set_enable(sensor, enable);
}

static int bmi160_gyro_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
			unsigned int delay_ms)
{
	struct bmi_client_data *sensor = container_of(sensors_cdev,
			struct bmi_client_data, gyro_cdev);

	return	bmi160_gyro_set_poll_delay(sensor, delay_ms);
}
#endif

int bmi_probe(struct bmi_client_data *client_data, struct device *dev)
{
	int err = 0;
#ifdef BMI160_MAG_INTERFACE_SUPPORT
	u8 mag_dev_addr;
	u8 mag_urst_len;
	u8 mag_op_mode;
#endif
	/* check chip id */
	err = bmi_check_chip_id(client_data);
	if (err)
		goto exit_err_clean;

	dev_set_drvdata(dev, client_data);
	client_data->dev = dev;

	mutex_init(&client_data->mutex_enable);
	mutex_init(&client_data->mutex_op_mode);

	/* input device init */
	err = bmi_input_init(client_data);
	if (err < 0)
		goto exit_err_clean;

	/* sysfs node creation */
	err = sysfs_create_group(&client_data->input->dev.kobj,
			&bmi160_attribute_group);

	if (err < 0)
		goto exit_err_sysfs;

	if (NULL != dev->platform_data) {
		client_data->bst_pd = kzalloc(sizeof(*client_data->bst_pd),
				GFP_KERNEL);

		if (NULL != client_data->bst_pd) {
			memcpy(client_data->bst_pd, dev->platform_data,
					sizeof(*client_data->bst_pd));
			dev_notice(dev, "%s sensor driver set place: p%d\n",
					client_data->bst_pd->name,
					client_data->bst_pd->place);
		}
	}

	if (NULL != client_data->bst_pd) {
			memcpy(client_data->bst_pd, dev->platform_data,
					sizeof(*client_data->bst_pd));
			dev_notice(dev, "%s sensor driver set place: p%d\n",
					client_data->bst_pd->name,
					client_data->bst_pd->place);
		}


	/* workqueue init */
	INIT_DELAYED_WORK(&client_data->work, bmi_work_func);
	atomic_set(&client_data->delay, BMI_DELAY_DEFAULT);
	atomic_set(&client_data->wkqueue_en, 0);

	/* h/w init */
	client_data->device.delay_msec = bmi_delay;
	err = BMI_CALL_API(init)(&client_data->device);

	bmi_dump_reg(client_data);

	/*power on detected*/
	/*or softrest(cmd 0xB6) */
	/*fatal err check*/
	/*soft reset*/
	err += BMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	bmi_delay(3);
	if (err)
		dev_err(dev, "Failed soft reset, er=%d", err);
	/*usr data config page*/
	err += BMI_CALL_API(set_target_page)(USER_DAT_CFG_PAGE);
	if (err)
		dev_err(dev, "Failed cffg page, er=%d", err);
	err += bmi_get_err_status(client_data);
	if (err) {
		dev_err(dev, "Failed to bmi16x init!err_st=0x%x\n",
				client_data->err_st.err_st_all);
		goto exit_err_sysfs;
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
		dev_err(client_data->dev, "Failed cffg mag, er=%d", err);
	dev_info(client_data->dev,
		"BMI160 mag_urst_len:%d, mag_add:0x%x, mag_op_mode:%d\n",
		mag_urst_len, mag_dev_addr, mag_op_mode);
#endif
	if (err < 0)
		goto exit_err_sysfs;


#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
		/*wake_lock_init(&client_data->wakelock,
			WAKE_LOCK_SUSPEND, "bmi160");*/
		client_data->gpio_pin = of_get_named_gpio_flags(dev->of_node,
					"bmi,gpio_irq", 0, NULL);
		dev_info(client_data->dev, "BMI160 qpio number:%d\n",
					client_data->gpio_pin);
		err += gpio_request_one(client_data->gpio_pin,
					GPIOF_IN, "bmi160_int");
		err += gpio_direction_input(client_data->gpio_pin);
		client_data->IRQ = gpio_to_irq(client_data->gpio_pin);
		if (err) {
			dev_err(client_data->dev,
				"can not request gpio to irq number\n");
			client_data->gpio_pin = 0;
		}
		INIT_DELAYED_WORK(&client_data->delay_work_sig,
			bmi160_delay_sigmo_work_func);
#ifdef BMI160_ENABLE_INT1
		/* maps interrupt to INT1/InT2 pin */
		BMI_CALL_API(set_intr_any_motion)(BMI_INT0, ENABLE);
		BMI_CALL_API(set_intr_fifo_wm)(BMI_INT0, ENABLE);
		BMI_CALL_API(set_intr_data_rdy)(BMI_INT0, ENABLE);

		/*Set interrupt trige level way */
		BMI_CALL_API(set_intr_edge_ctrl)(BMI_INT0, BMI_INT_LEVEL);
		bmi160_set_intr_level(BMI_INT0, 1);
		/*set interrupt latch temporary, 5 ms*/
		/*bmi160_set_latch_int(5);*/

		BMI_CALL_API(set_output_enable)(
		BMI160_INTR1_OUTPUT_ENABLE, ENABLE);
		sigmotion_init_interrupts(BMI160_MAP_INTR1);
		BMI_CALL_API(map_step_detector_intr)(BMI160_MAP_INTR1);
		/*close step_detector in init function*/
		BMI_CALL_API(set_step_detector_enable)(0);
#endif

#ifdef BMI160_ENABLE_INT2
		/* maps interrupt to INT1/InT2 pin */
		BMI_CALL_API(set_intr_any_motion)(BMI_INT1, ENABLE);
		BMI_CALL_API(set_intr_fifo_wm)(BMI_INT1, ENABLE);
		BMI_CALL_API(set_intr_data_rdy)(BMI_INT1, ENABLE);

		/*Set interrupt trige level way */
		BMI_CALL_API(set_intr_edge_ctrl)(BMI_INT1, BMI_INT_LEVEL);
		bmi160_set_intr_level(BMI_INT1, 1);
		/*set interrupt latch temporary, 5 ms*/
		/*bmi160_set_latch_int(5);*/

		BMI_CALL_API(set_output_enable)(
		BMI160_INTR2_OUTPUT_ENABLE, ENABLE);
		sigmotion_init_interrupts(BMI160_MAP_INTR2);
		BMI_CALL_API(map_step_detector_intr)(BMI160_MAP_INTR2);
		/*close step_detector in init function*/
		BMI_CALL_API(set_step_detector_enable)(0);
#endif
		err = request_irq(client_data->IRQ, bmi_irq_handler,
				IRQF_TRIGGER_RISING, "bmi160", client_data);
		if (err)
			dev_err(client_data->dev, "could not request irq\n");

		INIT_WORK(&client_data->irq_work, bmi_irq_work_func);
#endif

	client_data->selftest = 0;

	client_data->fifo_data_sel = 0;
	#if defined(CONFIG_USE_QUALCOMM_HAL)
	BMI_CALL_API(set_accel_output_data_rate)(9);/*defalut odr 200HZ*/
	BMI_CALL_API(set_gyro_output_data_rate)(9);/*defalut odr 200HZ*/
	#endif
	BMI_CALL_API(get_accel_output_data_rate)(&client_data->odr.acc_odr);
	BMI_CALL_API(get_gyro_output_data_rate)(&client_data->odr.gyro_odr);
	BMI_CALL_API(get_mag_output_data_rate)(&client_data->odr.mag_odr);
	BMI_CALL_API(set_fifo_time_enable)(1);
	BMI_CALL_API(get_accel_range)(&client_data->range.acc_range);
	BMI_CALL_API(get_gyro_range)(&client_data->range.gyro_range);
	/* now it's power on which is considered as resuming from suspend */
	
	/* gyro input device init */
	err = bmi_gyro_input_init(client_data);
	#if defined(CONFIG_USE_QUALCOMM_HAL)
	/* gyro input device init */
	err = bmi_gyro_input_init(client_data);
	if (err < 0)
		goto exit_err_clean;
	client_data->accel_poll_ms = BMI160_ACCEL_DEFAULT_POLL_INTERVAL_MS;
	client_data->gyro_poll_ms = BMI160_GYRO_DEFAULT_POLL_INTERVAL_MS;
	client_data->data_wq = create_freezable_workqueue("bmi160_data_work");
	if (!client_data->data_wq) {
		dev_err(dev, "Cannot create workqueue!\n");
		goto exit_err_clean;
	}
	INIT_DELAYED_WORK(&client_data->accel_poll_work,
		bmi160_accel_work_fn);
	client_data->accel_cdev = bmi160_accel_cdev;
	client_data->accel_cdev.delay_msec = client_data->accel_poll_ms;
	client_data->accel_cdev.sensors_enable = bmi160_accel_cdev_enable;
	client_data->accel_cdev.sensors_poll_delay =
	bmi160_accel_cdev_poll_delay;
	err = sensors_classdev_register(dev, &client_data->accel_cdev);
	if (err) {
		dev_err(dev,
			"create accel class device file failed!\n");
		goto exit_err_clean;
	}
	INIT_DELAYED_WORK(&client_data->gyro_poll_work, bmi160_gyro_work_fn);
	client_data->gyro_cdev = bmi160_gyro_cdev;
	client_data->gyro_cdev.delay_msec = client_data->gyro_poll_ms;
	client_data->gyro_cdev.sensors_enable = bmi160_gyro_cdev_enable;
	client_data->gyro_cdev.sensors_poll_delay = bmi160_gyro_cdev_poll_delay;
	err = sensors_classdev_register(dev, &client_data->gyro_cdev);
	if (err) {
		dev_err(dev,
			"create accel class device file failed!\n");
		goto exit_err_clean;
	}
	#endif
	/* set sensor PMU into suspend power mode for all */
	if (bmi_pmu_set_suspend(client_data) < 0) {
		dev_err(dev, "Failed to set BMI160 to suspend power mode\n");
		goto exit_err_sysfs;
	}
	/*enable the data ready interrupt*/
	BMI_CALL_API(set_intr_enable_1)(BMI160_DATA_RDY_ENABLE, 1);
	dev_notice(dev, "sensor_time:%d, %d, %d",
		sensortime_duration_tbl[0].ts_delat,
		sensortime_duration_tbl[0].ts_duration_lsb,
		sensortime_duration_tbl[0].ts_duration_us);
	dev_notice(dev, "sensor %s probed successfully", SENSOR_NAME);

	return 0;

exit_err_sysfs:
	if (err)
		bmi_input_destroy(client_data);

exit_err_clean:
	if (err) {
		if (client_data != NULL) {
			if (NULL != client_data->bst_pd) {
				kfree(client_data->bst_pd);
				client_data->bst_pd = NULL;
			}
		}
	}
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
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	if (NULL != client_data) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&client_data->early_suspend_handler);
#endif
		mutex_lock(&client_data->mutex_enable);
		if (BMI_ACC_PM_NORMAL == client_data->pw.acc_pm ||
			BMI_GYRO_PM_NORMAL == client_data->pw.gyro_pm ||
				BMI_MAG_PM_NORMAL == client_data->pw.mag_pm) {
			cancel_delayed_work_sync(&client_data->work);
		}
		mutex_unlock(&client_data->mutex_enable);

		err = bmi_pmu_set_suspend(client_data);

		bmi_delay(5);

		sysfs_remove_group(&client_data->input->dev.kobj,
				&bmi160_attribute_group);
		bmi_input_destroy(client_data);

		if (NULL != client_data->bst_pd) {
			kfree(client_data->bst_pd);
			client_data->bst_pd = NULL;
		}
		kfree(client_data);
	}

	return err;
}
EXPORT_SYMBOL(bmi_remove);

static int bmi_post_resume(struct bmi_client_data *client_data)
{
	int err = 0;

	mutex_lock(&client_data->mutex_enable);

	if (atomic_read(&client_data->wkqueue_en) == 1) {
		bmi160_set_acc_op_mode(client_data, BMI_ACC_PM_NORMAL);
		schedule_delayed_work(&client_data->work,
				msecs_to_jiffies(
					atomic_read(&client_data->delay)));
	}
	mutex_unlock(&client_data->mutex_enable);

	return err;
}


int bmi_suspend(struct device *dev)
{
	int err = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	unsigned char stc_enable;
	unsigned char std_enable;
	dev_err(client_data->dev, "bmi suspend function entrance");

	atomic_set(&client_data->in_suspend, 1);
	if (atomic_read(&client_data->wkqueue_en) == 1) {
		bmi160_set_acc_op_mode(client_data, BMI_ACC_PM_SUSPEND);
		cancel_delayed_work_sync(&client_data->work);
	}
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
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	atomic_set(&client_data->in_suspend, 0);
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
	/* post resume operation */
	err += bmi_post_resume(client_data);

	return err;
}
EXPORT_SYMBOL(bmi_resume);

