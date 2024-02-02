/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * (C) Modification Copyright 2018 Robert Bosch Kft  All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * Special: Description of the Software:
 *
 * This software module (hereinafter called "Software") and any
 * information on application-sheets (hereinafter called "Information") is
 * provided free of charge for the sole purpose to support your application
 * work. 
 *
 * As such, the Software is merely an experimental software, not tested for
 * safety in the field and only intended for inspiration for further development 
 * and testing. Any usage in a safety-relevant field of use (like automotive,
 * seafaring, spacefaring, industrial plants etc.) was not intended, so there are
 * no precautions for such usage incorporated in the Software.
 * 
 * The Software is specifically designed for the exclusive use for Bosch
 * Sensortec products by personnel who have special experience and training. Do
 * not use this Software if you do not have the proper experience or training.
 * 
 * This Software package is provided as is and without any expressed or
 * implied warranties, including without limitation, the implied warranties of
 * merchantability and fitness for a particular purpose.
 * 
 * Bosch Sensortec and their representatives and agents deny any liability for
 * the functional impairment of this Software in terms of fitness, performance
 * and safety. Bosch Sensortec and their representatives and agents shall not be
 * liable for any direct or indirect damages or injury, except as otherwise
 * stipulated in mandatory applicable law.
 * The Information provided is believed to be accurate and reliable. Bosch
 * Sensortec assumes no responsibility for the consequences of use of such
 * Information nor for any infringement of patents or other rights of third
 * parties which may result from its use.
 * 
 *------------------------------------------------------------------------------
 * The following Product Disclaimer does not apply to the BSX4-HAL-4.1NoFusion Software 
 * which is licensed under the Apache License, Version 2.0 as stated above.  
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Product Disclaimer
 *
 * Common:
 *
 * Assessment of Products Returned from Field
 *
 * Returned products are considered good if they fulfill the specifications / 
 * test data for 0-mileage and field listed in this document.
 *
 * Engineering Samples
 * 
 * Engineering samples are marked with (e) or (E). Samples may vary from the
 * valid technical specifications of the series product contained in this
 * data sheet. Therefore, they are not intended or fit for resale to
 * third parties or for use in end products. Their sole purpose is internal
 * client testing. The testing of an engineering sample may in no way replace
 * the testing of a series product. Bosch assumes no liability for the use
 * of engineering samples. The purchaser shall indemnify Bosch from all claims
 * arising from the use of engineering samples.
 *
 * Intended use
 *
 * Provided that SMI130 is used within the conditions (environment, application,
 * installation, loads) as described in this TCD and the corresponding
 * agreed upon documents, Bosch ensures that the product complies with
 * the agreed properties. Agreements beyond this require
 * the written approval by Bosch. The product is considered fit for the intended
 * use when the product successfully has passed the tests
 * in accordance with the TCD and agreed upon documents.
 *
 * It is the responsibility of the customer to ensure the proper application
 * of the product in the overall system/vehicle.
 *
 * Bosch does not assume any responsibility for changes to the environment
 * of the product that deviate from the TCD and the agreed upon documents 
 * as well as all applications not released by Bosch
  *
 * The resale and/or use of products are at the purchaser’s own risk and 
 * responsibility. The examination and testing of the SMI130 
 * is the sole responsibility of the purchaser.
 *
 * The purchaser shall indemnify Bosch from all third party claims 
 * arising from any product use not covered by the parameters of 
 * this product data sheet or not approved by Bosch and reimburse Bosch 
 * for all costs and damages in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products,
 * particularly with regard to product safety, and inform Bosch without delay
 * of all security relevant incidents.
 *
 * Application Examples and Hints
 *
 * With respect to any application examples, advice, normal values
 * and/or any information regarding the application of the device,
 * Bosch hereby disclaims any and all warranties and liabilities of any kind,
 * including without limitation warranties of
 * non-infringement of intellectual property rights or copyrights
 * of any third party.
 * The information given in this document shall in no event be regarded 
 * as a guarantee of conditions or characteristics. They are provided
 * for illustrative purposes only and no evaluation regarding infringement
 * of intellectual property rights or copyrights or regarding functionality,
 * performance or error has been made.
 *
 * @filename smi130_driver.c
 * @date     2016/08/01 14:40
 * @Modification Date 2018/08/28 18:20
 * @id       "b5ff23a"
 * @version  1.3
 *
 * @brief
 * The core code of SMI130 device driver
 *
 * @detail
 * This file implements the core code of SMI130 device driver,
 * which includes hardware related functions, input device register,
 * device attribute files, etc.
*/

#include "smi130.h"
#include "smi130_driver.h"
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>


#define DRIVER_VERSION "0.0.53.0"
#define I2C_BURST_READ_MAX_LEN      (256)
#define SMI130_STORE_COUNT  (6000)
#define LMADA     (1)
uint64_t g_current_apts_us_mbl;


enum SMI_SENSOR_INT_T {
	/* Interrupt enable0*/
	SMI_ANYMO_X_INT = 0,
	SMI_ANYMO_Y_INT,
	SMI_ANYMO_Z_INT,
	SMI_D_TAP_INT,
	SMI_S_TAP_INT,
	SMI_ORIENT_INT,
	SMI_FLAT_INT,
	/* Interrupt enable1*/
	SMI_HIGH_X_INT,
	SMI_HIGH_Y_INT,
	SMI_HIGH_Z_INT,
	SMI_LOW_INT,
	SMI_DRDY_INT,
	SMI_FFULL_INT,
	SMI_FWM_INT,
	/* Interrupt enable2 */
	SMI_NOMOTION_X_INT,
	SMI_NOMOTION_Y_INT,
	SMI_NOMOTION_Z_INT,
	SMI_STEP_DETECTOR_INT,
	INT_TYPE_MAX
};

/*smi fifo sensor type combination*/
enum SMI_SENSOR_FIFO_COMBINATION {
	SMI_FIFO_A = 0,
	SMI_FIFO_G,
	SMI_FIFO_M,
	SMI_FIFO_G_A,
	SMI_FIFO_M_A,
	SMI_FIFO_M_G,
	SMI_FIFO_M_G_A,
	SMI_FIFO_COM_MAX
};

/*smi fifo analyse return err status*/
enum SMI_FIFO_ANALYSE_RETURN_T {
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

/*!smi sensor generic power mode enum */
enum SMI_DEV_OP_MODE {
	SENSOR_PM_NORMAL = 0,
	SENSOR_PM_LP1,
	SENSOR_PM_SUSPEND,
	SENSOR_PM_LP2
};

/*! smi acc sensor power mode enum */
enum SMI_ACC_PM_TYPE {
	SMI_ACC_PM_NORMAL = 0,
	SMI_ACC_PM_LP1,
	SMI_ACC_PM_SUSPEND,
	SMI_ACC_PM_LP2,
	SMI_ACC_PM_MAX
};

/*! smi gyro sensor power mode enum */
enum SMI_GYRO_PM_TYPE {
	SMI_GYRO_PM_NORMAL = 0,
	SMI_GYRO_PM_FAST_START,
	SMI_GYRO_PM_SUSPEND,
	SMI_GYRO_PM_MAX
};

/*! smi mag sensor power mode enum */
enum SMI_MAG_PM_TYPE {
	SMI_MAG_PM_NORMAL = 0,
	SMI_MAG_PM_LP1,
	SMI_MAG_PM_SUSPEND,
	SMI_MAG_PM_LP2,
	SMI_MAG_PM_MAX
};


/*! smi sensor support type*/
enum SMI_SENSOR_TYPE {
	SMI_ACC_SENSOR,
	SMI_GYRO_SENSOR,
	SMI_MAG_SENSOR,
	SMI_SENSOR_TYPE_MAX
};

/*!smi sensor generic power mode enum */
enum SMI_AXIS_TYPE {
	X_AXIS = 0,
	Y_AXIS,
	Z_AXIS,
	AXIS_MAX
};

/*!smi sensor generic intterrupt enum */
enum SMI_INT_TYPE {
	SMI130_INT0 = 0,
	SMI130_INT1,
	SMI130_INT_MAX
};

/*! smi sensor time resolution definition*/
enum SMI_SENSOR_TIME_RS_TYPE {
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

/*! smi sensor interface mode */
enum SMI_SENSOR_IF_MODE_TYPE {
	/*primary interface:autoconfig/secondary interface off*/
	P_AUTO_S_OFF = 0,
	/*primary interface:I2C/secondary interface:OIS*/
	P_I2C_S_OIS,
	/*primary interface:autoconfig/secondary interface:Magnetometer*/
	P_AUTO_S_MAG,
	/*interface mode reseved*/
	IF_MODE_RESEVED

};

/*! smi130 acc/gyro calibration status in H/W layer */
enum SMI_CALIBRATION_STATUS_TYPE {
	/*SMI FAST Calibration ready x/y/z status*/
	SMI_ACC_X_FAST_CALI_RDY = 0,
	SMI_ACC_Y_FAST_CALI_RDY,
	SMI_ACC_Z_FAST_CALI_RDY
};

unsigned int reg_op_addr_mbl;

static const int smi_pmu_cmd_acc_arr[SMI_ACC_PM_MAX] = {
	/*!smi pmu for acc normal, low power1,
	 * suspend, low power2 mode command */
	CMD_PMU_ACC_NORMAL,
	CMD_PMU_ACC_LP1,
	CMD_PMU_ACC_SUSPEND,
	CMD_PMU_ACC_LP2
};

static const int smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_MAX] = {
	/*!smi pmu for gyro normal, fast startup,
	 * suspend mode command */
	CMD_PMU_GYRO_NORMAL,
	CMD_PMU_GYRO_FASTSTART,
	CMD_PMU_GYRO_SUSPEND
};

static const int smi_pmu_cmd_mag_arr[SMI_MAG_PM_MAX] = {
	/*!smi pmu for mag normal, low power1,
	 * suspend, low power2 mode command */
	CMD_PMU_MAG_NORMAL,
	CMD_PMU_MAG_LP1,
	CMD_PMU_MAG_SUSPEND,
	CMD_PMU_MAG_LP2
};

static const char *smi_axis_name[AXIS_MAX] = {"x", "y", "z"};

static const int smi_interrupt_type[] = {
	/*!smi interrupt type */
	/* Interrupt enable0 , index=0~6*/
	SMI130_ANY_MOTION_X_ENABLE,
	SMI130_ANY_MOTION_Y_ENABLE,
	SMI130_ANY_MOTION_Z_ENABLE,
	SMI130_DOUBLE_TAP_ENABLE,
	SMI130_SINGLE_TAP_ENABLE,
	SMI130_ORIENT_ENABLE,
	SMI130_FLAT_ENABLE,
	/* Interrupt enable1, index=7~13*/
	SMI130_HIGH_G_X_ENABLE,
	SMI130_HIGH_G_Y_ENABLE,
	SMI130_HIGH_G_Z_ENABLE,
	SMI130_LOW_G_ENABLE,
	SMI130_DATA_RDY_ENABLE,
	SMI130_FIFO_FULL_ENABLE,
	SMI130_FIFO_WM_ENABLE,
	/* Interrupt enable2, index = 14~17*/
	SMI130_NOMOTION_X_ENABLE,
	SMI130_NOMOTION_Y_ENABLE,
	SMI130_NOMOTION_Z_ENABLE,
	SMI130_STEP_DETECTOR_EN
};

/*! smi sensor time depend on ODR*/
struct smi_sensor_time_odr_tbl {
	u32 ts_duration_lsb;
	u32 ts_duration_us;
	u32 ts_delat;/*sub current delat fifo_time*/
};

struct smi130_axis_data_t {
	s16 x;
	s16 y;
	s16 z;
};

struct smi130_type_mapping_type {

	/*! smi16x sensor chip id */
	uint16_t chip_id;

	/*! smi16x chip revision code */
	uint16_t revision_id;

	/*! smi130_acc sensor name */
	const char *sensor_name;
};

struct smi130_store_info_t {
	uint8_t current_frm_cnt;
	uint64_t current_apts_us[2];
	uint8_t fifo_ts_total_frmcnt;
	uint64_t fifo_time;
};

uint64_t get_current_timestamp_mbl(void)
{
	uint64_t ts_ap;
	struct timespec tmp_time;
	get_monotonic_boottime(&tmp_time);
	ts_ap = (uint64_t)tmp_time.tv_sec * 1000000000 + tmp_time.tv_nsec;
	return ts_ap;

}

/*! sensor support type map */
static const struct smi130_type_mapping_type sensor_type_map[] = {

	{SENSOR_CHIP_ID_SMI, SENSOR_CHIP_REV_ID_SMI, "SMI130/162AB"},
	{SENSOR_CHIP_ID_SMI_C2, SENSOR_CHIP_REV_ID_SMI, "SMI130C2"},
	{SENSOR_CHIP_ID_SMI_C3, SENSOR_CHIP_REV_ID_SMI, "SMI130C3"},

};

/*!smi130 sensor time depends on ODR */
static const struct smi_sensor_time_odr_tbl
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
#define SMI130_ACCEL_MIN_VALUE	-32768
#define SMI130_ACCEL_MAX_VALUE	32767
#define SMI130_GYRO_MIN_VALUE	-32768
#define SMI130_GYRO_MAX_VALUE	32767
#define SMI130_ACCEL_DEFAULT_POLL_INTERVAL_MS	200
#define SMI130_GYRO_DEFAULT_POLL_INTERVAL_MS	200
#define SMI130_ACCEL_MIN_POLL_INTERVAL_MS	10
#define SMI130_ACCEL_MAX_POLL_INTERVAL_MS	5000
#define SMI130_GYRO_MIN_POLL_INTERVAL_MS	10
#define SMI130_GYRO_MAX_POLL_INTERVAL_MS	5000
static struct sensors_classdev smi130_accel_cdev = {
		.name = "smi130-accel",
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
static struct sensors_classdev smi130_gyro_cdev = {
	.name = "smi130-gyro",
	.vendor = "bosch",
	.version = 1,
	.handle = SENSORS_GYROSCOPE_HANDLE,
	.type = SENSOR_TYPE_GYROSCOPE,
	.max_range = "34.906586",	/* rad/s */
	.resolution = "0.0010681152",	/* rad/s */
	.sensor_power = "3.6",	/* 3.6 mA */
	.min_delay = SMI130_GYRO_MIN_POLL_INTERVAL_MS * 1000,
	.max_delay = SMI130_GYRO_MAX_POLL_INTERVAL_MS,
	.delay_msec = SMI130_GYRO_DEFAULT_POLL_INTERVAL_MS,
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
static void smi_delay(u32 msec)
{
	if (msec <= 20)
		usleep_range(msec * 1000, msec * 1000);
	else
		msleep(msec);
}

static void smi_dump_reg(struct smi_client_data *client_data)
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
			SMI_REG_NAME(USER_CHIP_ID), dbg_buf0, REG_MAX0);
	for (i = 0; i < REG_MAX0; i++) {
		snprintf(dbg_buf_str0 + i * 3, 16, "%02x%c", dbg_buf0[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_notice(client_data->dev, "%s\n", dbg_buf_str0);

	client_data->device.bus_read(client_data->device.dev_addr,
			SMI130_USER_ACCEL_CONFIG_ADDR, dbg_buf1, REG_MAX1);
	dev_notice(client_data->dev, "\nFrom 0x40:\n");
	for (i = 0; i < REG_MAX1; i++) {
		snprintf(dbg_buf_str1 + i * 3, 16, "%02x%c", dbg_buf1[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_notice(client_data->dev, "\n%s\n", dbg_buf_str1);
	}


void smi_fifo_frame_bytes_extend_calc(
	struct smi_client_data *client_data,
	unsigned int *fifo_frmbytes_extend)
{

	switch (client_data->fifo_data_sel) {
	case SMI_FIFO_A_SEL:
	case SMI_FIFO_G_SEL:
		*fifo_frmbytes_extend = 7;
		break;
	case SMI_FIFO_G_A_SEL:
		*fifo_frmbytes_extend = 13;
		break;
	case SMI_FIFO_M_SEL:
		*fifo_frmbytes_extend = 9;
		break;
	case SMI_FIFO_M_A_SEL:
	case SMI_FIFO_M_G_SEL:
		/*8(mag) + 6(gyro or acc) +1(head) = 15*/
		*fifo_frmbytes_extend = 15;
		break;
	case SMI_FIFO_M_G_A_SEL:
		/*8(mag) + 6(gyro or acc) + 6 + 1 = 21*/
		*fifo_frmbytes_extend = 21;
		break;
	default:
		*fifo_frmbytes_extend = 0;
		break;

	};

}

static int smi_input_init(struct smi_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;
#if defined(CONFIG_USE_QUALCOMM_HAL)
	dev->name = "smi130-accel";
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
	SMI130_ACCEL_MIN_VALUE, SMI130_ACCEL_MAX_VALUE,
	0, 0);
	input_set_abs_params(dev, ABS_Y,
	SMI130_ACCEL_MIN_VALUE, SMI130_ACCEL_MAX_VALUE,
	0, 0);
	input_set_abs_params(dev, ABS_Z,
	SMI130_ACCEL_MIN_VALUE, SMI130_ACCEL_MAX_VALUE,
	0, 0);
	#endif
	input_set_drvdata(dev, client_data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		dev_notice(client_data->dev, "smi130 input free!\n");
		return err;
	}
	client_data->input = dev;
	dev_notice(client_data->dev,
		"smi130 input register successfully, %s!\n",
		client_data->input->name);
	return err;
}

//#if defined(CONFIG_USE_QUALCOMM_HAL)
static int smi_gyro_input_init(struct smi_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;
	dev->name = "smi130-gyro";
	dev->id.bustype = BUS_I2C;
	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_capability(dev, EV_MSC, MSC_GESTURE);
	input_set_capability(dev, EV_MSC, INPUT_EVENT_SGM);
	
	input_set_capability(dev, EV_MSC, INPUT_EVENT_FAST_GYRO_CALIB_DONE);
	input_set_capability(dev, EV_MSC, INPUT_EVENT_STEP_DETECTOR);
	input_set_capability(dev, EV_MSC, INPUT_EVENT_FAST_ACC_CALIB_DONE);
	#if defined(CONFIG_USE_QUALCOMM_HAL)
	input_set_abs_params(dev, ABS_RX,
	SMI130_ACCEL_MIN_VALUE, SMI130_ACCEL_MAX_VALUE,
	0, 0);
	input_set_abs_params(dev, ABS_RY,
	SMI130_ACCEL_MIN_VALUE, SMI130_ACCEL_MAX_VALUE,
	0, 0);
	input_set_abs_params(dev, ABS_RZ,
	SMI130_ACCEL_MIN_VALUE, SMI130_ACCEL_MAX_VALUE,
	0, 0);
	#endif
	input_set_drvdata(dev, client_data);
	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		dev_notice(client_data->dev, "smi130 input free!\n");
		return err;
	}
	client_data->gyro_input = dev;
	dev_notice(client_data->dev,
		"smi130 input register successfully, %s!\n",
		client_data->gyro_input->name);
	return err;
}
//#endif
static void smi_input_destroy(struct smi_client_data *client_data)
{
	struct input_dev *dev = client_data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int smi_check_chip_id(struct smi_client_data *client_data)
{
	int8_t err = 0;
	int8_t i = 0;
	uint8_t chip_id = 0;
	uint8_t read_count = 0;
	u8 smi_sensor_cnt = sizeof(sensor_type_map)
				/ sizeof(struct smi130_type_mapping_type);
	/* read and check chip id */
	while (read_count++ < CHECK_CHIP_ID_TIME_MAX) {
		if (client_data->device.bus_read(client_data->device.dev_addr,
				SMI_REG_NAME(USER_CHIP_ID), &chip_id, 1) < 0) {

			dev_err(client_data->dev,
					"Bosch Sensortec Device not found"
						"read chip_id:%d\n", chip_id);
			continue;
		} else {
			for (i = 0; i < smi_sensor_cnt; i++) {
				if (sensor_type_map[i].chip_id == chip_id) {
					client_data->chip_id = chip_id;
					dev_notice(client_data->dev,
					"Bosch Sensortec Device detected, "
			"HW IC name: %s\n", sensor_type_map[i].sensor_name);
					break;
				}
			}
			if (i < smi_sensor_cnt)
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
			smi_delay(1);
		}
	}
	return err;

}

static int smi_pmu_set_suspend(struct smi_client_data *client_data)
{
	int err = 0;
	if (client_data == NULL)
		return -EINVAL;
	else {
		err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_acc_arr[SENSOR_PM_SUSPEND]);
		err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SENSOR_PM_SUSPEND]);
		err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_mag_arr[SENSOR_PM_SUSPEND]);
		client_data->pw.acc_pm = SMI_ACC_PM_SUSPEND;
		client_data->pw.gyro_pm = SMI_GYRO_PM_SUSPEND;
		client_data->pw.mag_pm = SMI_MAG_PM_SUSPEND;
	}

	return err;
}

static int smi_get_err_status(struct smi_client_data *client_data)
{
	int err = 0;

	err = SMI_CALL_API(get_error_status)(&client_data->err_st.fatal_err,
		&client_data->err_st.err_code, &client_data->err_st.i2c_fail,
	&client_data->err_st.drop_cmd, &client_data->err_st.mag_drdy_err);
	return err;
}

static void smi_work_func(struct work_struct *work)
{
	struct smi_client_data *client_data =
		container_of((struct delayed_work *)work,
			struct smi_client_data, work);
	unsigned long delay =
		msecs_to_jiffies(atomic_read(&client_data->delay));
	struct smi130_accel_t data;
	int err;

	err = SMI_CALL_API(read_accel_xyz)(&data);
	if (err < 0)
		return;

	/*report current frame via input event*/
	input_event(client_data->input, EV_REL, REL_X, data.x);
	input_event(client_data->input, EV_REL, REL_Y, data.y);
	input_event(client_data->input, EV_REL, REL_Z, data.z);
	input_sync(client_data->input);

	schedule_delayed_work(&client_data->work, delay);
}

static ssize_t smi130_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "0x%x\n", client_data->chip_id);
}

static ssize_t smi130_err_st_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	err = smi_get_err_status(client_data);
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

static ssize_t smi130_sensor_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	u32 sensor_time;
	err = SMI_CALL_API(get_sensor_time)(&sensor_time);
	if (err)
		return err;
	else
		return snprintf(buf, 16, "0x%x\n", (unsigned int)sensor_time);
}

static ssize_t smi130_fifo_flush_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long enable;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable)
		err = SMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);

	if (err)
		dev_err(client_data->dev, "fifo flush failed!\n");

	return count;

}


static ssize_t smi130_fifo_bytecount_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned int fifo_bytecount = 0;

	SMI_CALL_API(fifo_length)(&fifo_bytecount);
	err = snprintf(buf, 16, "%u\n", fifo_bytecount);
	return err;
}

static ssize_t smi130_fifo_bytecount_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	client_data->fifo_bytecount = (unsigned int) data;

	return count;
}

int smi130_fifo_data_sel_get(struct smi_client_data *client_data)
{
	int err = 0;
	unsigned char fifo_acc_en, fifo_gyro_en, fifo_mag_en;
	unsigned char fifo_datasel;

	err += SMI_CALL_API(get_fifo_accel_enable)(&fifo_acc_en);
	err += SMI_CALL_API(get_fifo_gyro_enable)(&fifo_gyro_en);
	err += SMI_CALL_API(get_fifo_mag_enable)(&fifo_mag_en);

	if (err)
		return err;

	fifo_datasel = (fifo_acc_en << SMI_ACC_SENSOR) |
			(fifo_gyro_en << SMI_GYRO_SENSOR) |
				(fifo_mag_en << SMI_MAG_SENSOR);

	client_data->fifo_data_sel = fifo_datasel;

	return err;


}

static ssize_t smi130_fifo_data_sel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	err = smi130_fifo_data_sel_get(client_data);
	if (err) {
		dev_err(client_data->dev, "get fifo_sel failed!\n");
		return -EINVAL;
	}
	return snprintf(buf, 16, "%d\n", client_data->fifo_data_sel);
}

/* write any value to clear all the fifo data. */
static ssize_t smi130_fifo_data_sel_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
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


	err += SMI_CALL_API(set_fifo_accel_enable)
			((fifo_datasel & (1 << SMI_ACC_SENSOR)) ? 1 :  0);
	err += SMI_CALL_API(set_fifo_gyro_enable)
			(fifo_datasel & (1 << SMI_GYRO_SENSOR) ? 1 : 0);
	err += SMI_CALL_API(set_fifo_mag_enable)
			((fifo_datasel & (1 << SMI_MAG_SENSOR)) ? 1 : 0);

	err += SMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);
	if (err)
		return -EIO;
	else {
		dev_notice(client_data->dev, "FIFO A_en:%d, G_en:%d, M_en:%d\n",
			(fifo_datasel & (1 << SMI_ACC_SENSOR)) ? 1 :  0,
			(fifo_datasel & (1 << SMI_GYRO_SENSOR) ? 1 : 0),
			((fifo_datasel & (1 << SMI_MAG_SENSOR)) ? 1 : 0));
		client_data->fifo_data_sel = fifo_datasel;
	}
	return count;
}

static ssize_t smi130_fifo_data_out_frame_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	int err = 0;
	uint32_t fifo_bytecount = 0;

	err = SMI_CALL_API(fifo_length)(&fifo_bytecount);
	if (err < 0) {
		dev_err(client_data->dev, "read fifo_length err");
		return -EINVAL;
	}
	if (fifo_bytecount == 0)
		return 0;
	err = smi_burst_read_wrapper(client_data->device.dev_addr,
		SMI130_USER_FIFO_DATA__REG, buf,
		fifo_bytecount);
	if (err) {
		dev_err(client_data->dev, "read fifo err");
		SMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);
		return -EINVAL;
	}
	return fifo_bytecount;

}

static ssize_t smi130_fifo_watermark_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0xff;

	err = SMI_CALL_API(get_fifo_wm)(&data);

	if (err)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_fifo_watermark_store(struct device *dev,
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
	err = SMI_CALL_API(set_fifo_wm)(fifo_watermark);
	if (err)
		return -EIO;

	return count;
}


static ssize_t smi130_fifo_header_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0xff;

	err = SMI_CALL_API(get_fifo_header_enable)(&data);

	if (err)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_fifo_header_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;
	unsigned char fifo_header_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	if (data > 1)
		return -ENOENT;

	fifo_header_en = (unsigned char)data;
	err = SMI_CALL_API(set_fifo_header_enable)(fifo_header_en);
	if (err)
		return -EIO;

	client_data->fifo_head_en = fifo_header_en;

	return count;
}

static ssize_t smi130_fifo_time_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0;

	err = SMI_CALL_API(get_fifo_time_enable)(&data);

	if (!err)
		err = snprintf(buf, 16, "%d\n", data);

	return err;
}

static ssize_t smi130_fifo_time_en_store(struct device *dev,
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

	err = SMI_CALL_API(set_fifo_time_enable)(fifo_ts_en);
	if (err)
		return -EIO;

	return count;
}

static ssize_t smi130_fifo_int_tag_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char fifo_tag_int1 = 0;
	unsigned char fifo_tag_int2 = 0;
	unsigned char fifo_tag_int;

	err += SMI_CALL_API(get_fifo_tag_intr1_enable)(&fifo_tag_int1);
	err += SMI_CALL_API(get_fifo_tag_intr2_enable)(&fifo_tag_int2);

	fifo_tag_int = (fifo_tag_int1 << SMI130_INT0) |
			(fifo_tag_int2 << SMI130_INT1);

	if (!err)
		err = snprintf(buf, 16, "%d\n", fifo_tag_int);

	return err;
}

static ssize_t smi130_fifo_int_tag_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;
	unsigned char fifo_tag_int_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	if (data > 3)
		return -EINVAL;

	fifo_tag_int_en = (unsigned char)data;

	err += SMI_CALL_API(set_fifo_tag_intr1_enable)
			((fifo_tag_int_en & (1 << SMI130_INT0)) ? 1 :  0);
	err += SMI_CALL_API(set_fifo_tag_intr2_enable)
			((fifo_tag_int_en & (1 << SMI130_INT1)) ? 1 :  0);

	if (err) {
		dev_err(client_data->dev, "fifo int tag en err:%d\n", err);
		return -EIO;
	}
	client_data->fifo_int_tag_en = fifo_tag_int_en;

	return count;
}

static int smi130_set_acc_op_mode(struct smi_client_data *client_data,
							unsigned long op_mode)
{
	int err = 0;
	unsigned char stc_enable;
	unsigned char std_enable;
	mutex_lock(&client_data->mutex_op_mode);

	if (op_mode < SMI_ACC_PM_MAX) {
		switch (op_mode) {
		case SMI_ACC_PM_NORMAL:
			err = SMI_CALL_API(set_command_register)
			(smi_pmu_cmd_acc_arr[SMI_ACC_PM_NORMAL]);
			client_data->pw.acc_pm = SMI_ACC_PM_NORMAL;
			smi_delay(10);
			break;
		case SMI_ACC_PM_LP1:
			err = SMI_CALL_API(set_command_register)
			(smi_pmu_cmd_acc_arr[SMI_ACC_PM_LP1]);
			client_data->pw.acc_pm = SMI_ACC_PM_LP1;
			smi_delay(3);
			break;
		case SMI_ACC_PM_SUSPEND:
			SMI_CALL_API(get_step_counter_enable)(&stc_enable);
			SMI_CALL_API(get_step_detector_enable)(&std_enable);
			if ((stc_enable == 0) && (std_enable == 0) &&
				(client_data->sig_flag == 0)) {
				err = SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_acc_arr[SMI_ACC_PM_SUSPEND]);
				client_data->pw.acc_pm = SMI_ACC_PM_SUSPEND;
				smi_delay(10);
			}
			break;
		case SMI_ACC_PM_LP2:
			err = SMI_CALL_API(set_command_register)
			(smi_pmu_cmd_acc_arr[SMI_ACC_PM_LP2]);
			client_data->pw.acc_pm = SMI_ACC_PM_LP2;
			smi_delay(3);
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

static ssize_t smi130_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	s16 temp = 0xff;

	err = SMI_CALL_API(get_temp)(&temp);

	if (!err)
		err = snprintf(buf, 16, "0x%x\n", temp);

	return err;
}

static ssize_t smi130_place_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

	if (NULL != client_data->bosch_pd)
		place = client_data->bosch_pd->place;

	return snprintf(buf, 16, "%d\n", place);
}

static ssize_t smi130_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "%d\n", atomic_read(&client_data->delay));

}

static ssize_t smi130_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long data;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	if (data == 0) {
		err = -EINVAL;
		return err;
	}

	if (data < SMI_DELAY_MIN)
		data = SMI_DELAY_MIN;

	atomic_set(&client_data->delay, (unsigned int)data);

	return count;
}

static ssize_t smi130_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "%d\n", atomic_read(&client_data->wkqueue_en));

}

static ssize_t smi130_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
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
			smi130_set_acc_op_mode(client_data,
							SMI_ACC_PM_NORMAL);
			schedule_delayed_work(&client_data->work,
			msecs_to_jiffies(atomic_read(&client_data->delay)));
			atomic_set(&client_data->wkqueue_en, 1);
		}

	} else {
		if (pre_enable == 1) {
			smi130_set_acc_op_mode(client_data,
							SMI_ACC_PM_SUSPEND);

			cancel_delayed_work_sync(&client_data->work);
			atomic_set(&client_data->wkqueue_en, 0);
		}
	}

	mutex_unlock(&client_data->mutex_enable);

	return count;
}

#if defined(SMI130_ENABLE_INT1) || defined(SMI130_ENABLE_INT2)
/* accel sensor part */
static ssize_t smi130_anymot_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data;

	err = SMI_CALL_API(get_intr_any_motion_durn)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_anymot_duration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_intr_any_motion_durn)((unsigned char)data);
	if (err < 0)
		return -EIO;

	return count;
}

static ssize_t smi130_anymot_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = SMI_CALL_API(get_intr_any_motion_thres)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_anymot_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_intr_any_motion_thres)((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t smi130_step_detector_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 data = 0;
	u8 step_det;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	err = SMI_CALL_API(get_step_detector_enable)(&step_det);
	/*smi130_get_status0_step_int*/
	if (err < 0)
		return err;
/*client_data->std will be updated in smi_stepdetector_interrupt_handle */
	if ((step_det == 1) && (client_data->std == 1)) {
		data = 1;
		client_data->std = 0;
		}
	else {
		data = 0;
		}
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_step_detector_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = SMI_CALL_API(get_step_detector_enable)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_step_detector_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_step_detector_enable)((unsigned char)data);
	if (err < 0)
		return -EIO;
	if (data == 0)
		client_data->pedo_data.wkar_step_detector_status = 0;
	return count;
}

static ssize_t smi130_signification_motion_enable_store(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/*0x62 (bit 1) INT_MOTION_3 int_sig_mot_sel*/
	err = SMI_CALL_API(set_intr_significant_motion_select)(
		(unsigned char)data);
	if (err < 0)
		return -EIO;
	if (data == 1) {
		err = SMI_CALL_API(set_intr_enable_0)
					(SMI130_ANY_MOTION_X_ENABLE, 1);
		err += SMI_CALL_API(set_intr_enable_0)
					(SMI130_ANY_MOTION_Y_ENABLE, 1);
		err += SMI_CALL_API(set_intr_enable_0)
					(SMI130_ANY_MOTION_Z_ENABLE, 1);
		if (err < 0)
			return -EIO;
		enable_irq_wake(client_data->IRQ);
		client_data->sig_flag = 1;
	} else {
		err = SMI_CALL_API(set_intr_enable_0)
					(SMI130_ANY_MOTION_X_ENABLE, 0);
		err += SMI_CALL_API(set_intr_enable_0)
					(SMI130_ANY_MOTION_Y_ENABLE, 0);
		err += SMI_CALL_API(set_intr_enable_0)
					(SMI130_ANY_MOTION_Z_ENABLE, 0);
		if (err < 0)
			return -EIO;
		disable_irq_wake(client_data->IRQ);
		client_data->sig_flag = 0;
	}
	return count;
}

static ssize_t smi130_signification_motion_enable_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;
	/*0x62 (bit 1) INT_MOTION_3 int_sig_mot_sel*/
	err = SMI_CALL_API(get_intr_significant_motion_select)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static int sigmotion_init_interrupts(u8 sig_map_int_pin)
{
	int ret = 0;
/*0x60  */
	ret += smi130_set_intr_any_motion_thres(0x1e);
/* 0x62(bit 3~2)	0=1.5s */
	ret += smi130_set_intr_significant_motion_skip(0);
/*0x62(bit 5~4)	1=0.5s*/
	ret += smi130_set_intr_significant_motion_proof(1);
/*0x50 (bit 0, 1, 2)  INT_EN_0 anymo x y z*/
	ret += smi130_map_significant_motion_intr(sig_map_int_pin);
/*0x62 (bit 1) INT_MOTION_3	int_sig_mot_sel
close the signification_motion*/
	ret += smi130_set_intr_significant_motion_select(0);
/*close the anymotion interrupt*/
	ret += SMI_CALL_API(set_intr_enable_0)
					(SMI130_ANY_MOTION_X_ENABLE, 0);
	ret += SMI_CALL_API(set_intr_enable_0)
					(SMI130_ANY_MOTION_Y_ENABLE, 0);
	ret += SMI_CALL_API(set_intr_enable_0)
					(SMI130_ANY_MOTION_Z_ENABLE, 0);
	if (ret)
		printk(KERN_ERR "smi130 sig motion failed setting,%d!\n", ret);
	return ret;

}
#endif

static ssize_t smi130_acc_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char range;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = SMI_CALL_API(get_accel_range)(&range);
	if (err)
		return err;

	client_data->range.acc_range = range;
	return snprintf(buf, 16, "%d\n", range);
}

static ssize_t smi130_acc_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);


	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;

	err = SMI_CALL_API(set_accel_range)(range);
	if (err)
		return -EIO;

	client_data->range.acc_range = range;
	return count;
}

static ssize_t smi130_acc_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char acc_odr;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = SMI_CALL_API(get_accel_output_data_rate)(&acc_odr);
	if (err)
		return err;

	client_data->odr.acc_odr = acc_odr;
	return snprintf(buf, 16, "%d\n", acc_odr);
}

static ssize_t smi130_acc_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long acc_odr;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &acc_odr);
	if (err)
		return err;

	if (acc_odr < 1 || acc_odr > 12)
		return -EIO;

	if (acc_odr < 5)
		err = SMI_CALL_API(set_accel_under_sampling_parameter)(1);
	else
		err = SMI_CALL_API(set_accel_under_sampling_parameter)(0);

	if (err)
		return err;

	err = SMI_CALL_API(set_accel_output_data_rate)(acc_odr);
	if (err)
		return -EIO;
	client_data->odr.acc_odr = acc_odr;
	return count;
}

static ssize_t smi130_acc_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	u8 accel_pmu_status = 0;
	err = SMI_CALL_API(get_accel_power_mode_stat)(
		&accel_pmu_status);

	if (err)
		return err;
	else
	return snprintf(buf, 32, "reg:%d, val:%d\n", accel_pmu_status,
			client_data->pw.acc_pm);
}

static ssize_t smi130_acc_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	int err;
	unsigned long op_mode;
	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	err = smi130_set_acc_op_mode(client_data, op_mode);
	if (err)
		return err;
	else
		return count;

}

static ssize_t smi130_acc_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct smi130_accel_t data;

	int err;

	err = SMI_CALL_API(read_accel_xyz)(&data);
	if (err < 0)
		return err;

	return snprintf(buf, 48, "%hd %hd %hd\n",
			data.x, data.y, data.z);
}

static ssize_t smi130_acc_fast_calibration_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = SMI_CALL_API(get_foc_accel_x)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_acc_fast_calibration_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	s8 accel_offset_x = 0;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = SMI_CALL_API(set_accel_foc_trigger)(X_AXIS,
					data, &accel_offset_x);
	if (err)
		return -EIO;
	else
		client_data->calib_status |=
			SMI_FAST_CALI_TRUE << SMI_ACC_X_FAST_CALI_RDY;
	return count;
}

static ssize_t smi130_acc_fast_calibration_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = SMI_CALL_API(get_foc_accel_y)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_acc_fast_calibration_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	s8 accel_offset_y = 0;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = SMI_CALL_API(set_accel_foc_trigger)(Y_AXIS,
				data, &accel_offset_y);
	if (err)
		return -EIO;
	else
		client_data->calib_status |=
			SMI_FAST_CALI_TRUE << SMI_ACC_Y_FAST_CALI_RDY;
	return count;
}

static ssize_t smi130_acc_fast_calibration_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = SMI_CALL_API(get_foc_accel_z)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_acc_fast_calibration_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	s8 accel_offset_z = 0;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	unsigned char data1[3] = {0};
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = SMI_CALL_API(set_accel_foc_trigger)(Z_AXIS,
			data, &accel_offset_z);
	if (err)
		return -EIO;
	else
		client_data->calib_status |=
			SMI_FAST_CALI_TRUE << SMI_ACC_Z_FAST_CALI_RDY;

	if (client_data->calib_status == SMI_FAST_CALI_ALL_RDY) {
		err = SMI_CALL_API(get_accel_offset_compensation_xaxis)(
			&data1[0]);
		err += SMI_CALL_API(get_accel_offset_compensation_yaxis)(
			&data1[1]);
		err += SMI_CALL_API(get_accel_offset_compensation_zaxis)(
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

static ssize_t smi130_acc_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = SMI_CALL_API(get_accel_offset_compensation_xaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}


static ssize_t smi130_acc_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_accel_offset_compensation_xaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t smi130_acc_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = SMI_CALL_API(get_accel_offset_compensation_yaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_acc_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_accel_offset_compensation_yaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t smi130_acc_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = SMI_CALL_API(get_accel_offset_compensation_zaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_acc_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_accel_offset_compensation_zaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t smi130_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	u8 raw_data[15] = {0};
	unsigned int sensor_time = 0;

	int err;
	memset(raw_data, 0, sizeof(raw_data));

	err = client_data->device.bus_read(client_data->device.dev_addr,
			SMI130_USER_DATA_8_GYRO_X_LSB__REG, raw_data, 15);
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

static ssize_t smi130_step_counter_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = SMI_CALL_API(get_step_counter_enable)(&data);

	client_data->stc_enable = data;

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_step_counter_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_step_counter_enable)((unsigned char)data);

	client_data->stc_enable = data;

	if (err < 0)
		return -EIO;
	return count;
}


static ssize_t smi130_step_counter_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_step_mode)((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t smi130_step_counter_clc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = smi130_clear_step_counter();

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t smi130_step_counter_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u16 data;
	int err;
	static u16 last_stc_value;

	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = SMI_CALL_API(read_step_count)(&data);

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

static ssize_t smi130_smi_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	u8 raw_data[12] = {0};

	int err;
	memset(raw_data, 0, sizeof(raw_data));

	err = client_data->device.bus_read(client_data->device.dev_addr,
			SMI130_USER_DATA_8_GYRO_X_LSB__REG, raw_data, 12);
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


static ssize_t smi130_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	return snprintf(buf, 16, "0x%x\n",
				atomic_read(&client_data->selftest_result));
}

static int smi_restore_hw_cfg(struct smi_client_data *client);

/*!
 * @brief store selftest result which make up of acc and gyro
 * format: 0b 0000 xxxx  x:1 failed, 0 success
 * bit3:     gyro_self
 * bit2..0: acc_self z y x
 */
static ssize_t smi130_selftest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	int i = 0;

	u8 acc_selftest = 0;
	u8 gyro_selftest = 0;
	u8 smi_selftest = 0;
	s16 axis_p_value, axis_n_value;
	u16 diff_axis[3] = {0xff, 0xff, 0xff};
	u8 acc_odr, range, acc_selftest_amp, acc_selftest_sign;

	dev_notice(client_data->dev, "Selftest for SMI16x starting.\n");

	client_data->selftest = 1;

	/*soft reset*/
	err = SMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	msleep(70);
	err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_acc_arr[SMI_ACC_PM_NORMAL]);
	err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_NORMAL]);
	err += SMI_CALL_API(set_accel_under_sampling_parameter)(0);
	err += SMI_CALL_API(set_accel_output_data_rate)(
	SMI130_ACCEL_OUTPUT_DATA_RATE_1600HZ);

	/* set to 8G range*/
	err += SMI_CALL_API(set_accel_range)(SMI130_ACCEL_RANGE_8G);
	/* set to self amp high */
	err += SMI_CALL_API(set_accel_selftest_amp)(SMI_SELFTEST_AMP_HIGH);


	err += SMI_CALL_API(get_accel_output_data_rate)(&acc_odr);
	err += SMI_CALL_API(get_accel_range)(&range);
	err += SMI_CALL_API(get_accel_selftest_amp)(&acc_selftest_amp);
	err += SMI_CALL_API(read_accel_x)(&axis_n_value);

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
		err += SMI_CALL_API(set_accel_selftest_axis)(i + 1);
		msleep(50);
		switch (i) {
		case X_AXIS:
			/* set negative sign */
			err += SMI_CALL_API(set_accel_selftest_sign)(0);
			err += SMI_CALL_API(get_accel_selftest_sign)(
				&acc_selftest_sign);

			msleep(60);
			err += SMI_CALL_API(read_accel_x)(&axis_n_value);
			dev_info(client_data->dev,
			"acc_x_selftest_sign:%d, axis_n_value:%d\n",
			acc_selftest_sign, axis_n_value);

			/* set postive sign */
			err += SMI_CALL_API(set_accel_selftest_sign)(1);
			err += SMI_CALL_API(get_accel_selftest_sign)(
				&acc_selftest_sign);

			msleep(60);
			err += SMI_CALL_API(read_accel_x)(&axis_p_value);
			dev_info(client_data->dev,
			"acc_x_selftest_sign:%d, axis_p_value:%d\n",
			acc_selftest_sign, axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;

		case Y_AXIS:
			/* set negative sign */
			err += SMI_CALL_API(set_accel_selftest_sign)(0);
			msleep(60);
			err += SMI_CALL_API(read_accel_y)(&axis_n_value);
			/* set postive sign */
			err += SMI_CALL_API(set_accel_selftest_sign)(1);
			msleep(60);
			err += SMI_CALL_API(read_accel_y)(&axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;

		case Z_AXIS:
			/* set negative sign */
			err += SMI_CALL_API(set_accel_selftest_sign)(0);
			msleep(60);
			err += SMI_CALL_API(read_accel_z)(&axis_n_value);
			/* set postive sign */
			err += SMI_CALL_API(set_accel_selftest_sign)(1);
			msleep(60);
			err += SMI_CALL_API(read_accel_z)(&axis_p_value);
			/* also start gyro self test */
			err += SMI_CALL_API(set_gyro_selftest_start)(1);
			msleep(60);
			err += SMI_CALL_API(get_gyro_selftest)(&gyro_selftest);

			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;
		default:
			err += -EINVAL;
			break;
		}
		if (err) {
			dev_err(client_data->dev,
				"Failed selftest axis:%s, p_val=%d, n_val=%d\n",
				smi_axis_name[i], axis_p_value, axis_n_value);
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
					smi_axis_name[i], diff_axis[i],
						axis_p_value, axis_n_value);
			}
		} else {
			/*800mg for x or y axis*/
			if (diff_axis[i] < 3277) {
				acc_selftest |= 1 << i;

				if (smi_get_err_status(client_data) < 0)
					return err;
				dev_err(client_data->dev,
					"Over selftest minimum for "
					"axis:%s,diff=%d, p_val=%d, n_val=%d\n",
					smi_axis_name[i], diff_axis[i],
						axis_p_value, axis_n_value);
				dev_err(client_data->dev, "err_st:0x%x\n",
						client_data->err_st.err_st_all);

			}
		}

	}
	/* gyro_selftest==1,gyro selftest successfully,
	* but smi_result bit4 0 is successful, 1 is failed*/
	smi_selftest = (acc_selftest & 0x0f) | ((!gyro_selftest) << AXIS_MAX);
	atomic_set(&client_data->selftest_result, smi_selftest);
	/*soft reset*/
	err = SMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	if (err) {
		client_data->selftest = 0;
		return err;
	}
	msleep(50);

	smi_restore_hw_cfg(client_data);

	client_data->selftest = 0;
	dev_notice(client_data->dev, "Selftest for SMI16x finished\n");

	return count;
}

/* gyro sensor part */
static ssize_t smi130_gyro_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	int err = 0;
	u8 gyro_pmu_status = 0;

	err = SMI_CALL_API(get_gyro_power_mode_stat)(
		&gyro_pmu_status);

	if (err)
		return err;
	else
	return snprintf(buf, 32, "reg:%d, val:%d\n", gyro_pmu_status,
				client_data->pw.gyro_pm);
}

static ssize_t smi130_gyro_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	unsigned long op_mode;
	int err;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	mutex_lock(&client_data->mutex_op_mode);

	if (op_mode < SMI_GYRO_PM_MAX) {
		switch (op_mode) {
		case SMI_GYRO_PM_NORMAL:
			err = SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_NORMAL]);
			client_data->pw.gyro_pm = SMI_GYRO_PM_NORMAL;
			smi_delay(60);
			break;
		case SMI_GYRO_PM_FAST_START:
			err = SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_FAST_START]);
			client_data->pw.gyro_pm = SMI_GYRO_PM_FAST_START;
			smi_delay(60);
			break;
		case SMI_GYRO_PM_SUSPEND:
			err = SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_SUSPEND]);
			client_data->pw.gyro_pm = SMI_GYRO_PM_SUSPEND;
			smi_delay(60);
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

static ssize_t smi130_gyro_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct smi130_gyro_t data;
	int err;

	err = SMI_CALL_API(read_gyro_xyz)(&data);
	if (err < 0)
		return err;


	return snprintf(buf, 48, "%hd %hd %hd\n", data.x,
				data.y, data.z);
}

static ssize_t smi130_gyro_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char range;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = SMI_CALL_API(get_gyro_range)(&range);
	if (err)
		return err;

	client_data->range.gyro_range = range;
	return snprintf(buf, 16, "%d\n", range);
}

static ssize_t smi130_gyro_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;

	err = SMI_CALL_API(set_gyro_range)(range);
	if (err)
		return -EIO;

	client_data->range.gyro_range = range;
	return count;
}

static ssize_t smi130_gyro_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char gyro_odr;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = SMI_CALL_API(get_gyro_output_data_rate)(&gyro_odr);
	if (err)
		return err;

	client_data->odr.gyro_odr = gyro_odr;
	return snprintf(buf, 16, "%d\n", gyro_odr);
}

static ssize_t smi130_gyro_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long gyro_odr;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &gyro_odr);
	if (err)
		return err;

	if (gyro_odr < 6 || gyro_odr > 13)
		return -EIO;

	err = SMI_CALL_API(set_gyro_output_data_rate)(gyro_odr);
	if (err)
		return -EIO;

	client_data->odr.gyro_odr = gyro_odr;
	return count;
}

static ssize_t smi130_gyro_fast_calibration_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = SMI_CALL_API(get_foc_gyro_enable)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_gyro_fast_calibration_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long enable;
	s8 err;
	s16 gyr_off_x;
	s16 gyr_off_y;
	s16 gyr_off_z;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;

	err = SMI_CALL_API(set_foc_gyro_enable)((u8)enable,
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

static ssize_t smi130_gyro_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data = 0;
	s8 err = 0;

	err = SMI_CALL_API(get_gyro_offset_compensation_xaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_gyro_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	s8 err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_gyro_offset_compensation_xaxis)((s16)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t smi130_gyro_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data = 0;
	s8 err = 0;

	err = SMI_CALL_API(get_gyro_offset_compensation_yaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_gyro_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	s8 err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_gyro_offset_compensation_yaxis)((s16)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t smi130_gyro_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data = 0;
	int err = 0;

	err = SMI_CALL_API(get_gyro_offset_compensation_zaxis)(&data);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t smi130_gyro_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = SMI_CALL_API(set_gyro_offset_compensation_zaxis)((s16)data);

	if (err < 0)
		return -EIO;
	return count;
}


/* mag sensor part */
#ifdef SMI130_MAG_INTERFACE_SUPPORT
static ssize_t smi130_mag_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	u8 mag_op_mode;
	s8 err;
	err = smi130_get_mag_power_mode_stat(&mag_op_mode);
	if (err) {
		dev_err(client_data->dev,
			"Failed to get SMI130 mag power mode:%d\n", err);
		return err;
	} else
		return snprintf(buf, 32, "%d, reg:%d\n",
					client_data->pw.mag_pm, mag_op_mode);
}

static ssize_t smi130_mag_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	unsigned long op_mode;
	int err;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	if (op_mode == client_data->pw.mag_pm)
		return count;

	mutex_lock(&client_data->mutex_op_mode);


	if (op_mode < SMI_MAG_PM_MAX) {
		switch (op_mode) {
		case SMI_MAG_PM_NORMAL:
			/* need to modify as mag sensor connected,
			 * set write address to 0x4c and triggers
			 * write operation
			 * 0x4c(op mode control reg)
			 * enables normal mode in magnetometer */
#if defined(SMI130_AKM09912_SUPPORT)
			err = smi130_set_bosch_akm_and_secondary_if_powermode(
			SMI130_MAG_FORCE_MODE);
#else
			err = smi130_set_bmm150_mag_and_secondary_if_power_mode(
			SMI130_MAG_FORCE_MODE);
#endif
			client_data->pw.mag_pm = SMI_MAG_PM_NORMAL;
			smi_delay(5);
			break;
		case SMI_MAG_PM_LP1:
			/* need to modify as mag sensor connected,
			 * set write address to 0x4 band triggers
			 * write operation
			 * 0x4b(bmm150, power control reg, bit0)
			 * enables power in magnetometer*/
#if defined(SMI130_AKM09912_SUPPORT)
			err = smi130_set_bosch_akm_and_secondary_if_powermode(
			SMI130_MAG_FORCE_MODE);
#else
			err = smi130_set_bmm150_mag_and_secondary_if_power_mode(
			SMI130_MAG_FORCE_MODE);
#endif
			client_data->pw.mag_pm = SMI_MAG_PM_LP1;
			smi_delay(5);
			break;
		case SMI_MAG_PM_SUSPEND:
		case SMI_MAG_PM_LP2:
#if defined(SMI130_AKM09912_SUPPORT)
		err = smi130_set_bosch_akm_and_secondary_if_powermode(
		SMI130_MAG_SUSPEND_MODE);
#else
		err = smi130_set_bmm150_mag_and_secondary_if_power_mode(
		SMI130_MAG_SUSPEND_MODE);
#endif
			client_data->pw.mag_pm = op_mode;
			smi_delay(5);
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
			"Failed to switch SMI130 mag power mode:%d\n",
			client_data->pw.mag_pm);
		return err;
	} else
		return count;

}

static ssize_t smi130_mag_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char mag_odr = 0;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = SMI_CALL_API(get_mag_output_data_rate)(&mag_odr);
	if (err)
		return err;

	client_data->odr.mag_odr = mag_odr;
	return snprintf(buf, 16, "%d\n", mag_odr);
}

static ssize_t smi130_mag_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long mag_odr;
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &mag_odr);
	if (err)
		return err;
	/*1~25/32hz,..6(25hz),7(50hz),... */
	err = SMI_CALL_API(set_mag_output_data_rate)(mag_odr);
	if (err)
		return -EIO;

	client_data->odr.mag_odr = mag_odr;
	return count;
}

static ssize_t smi130_mag_i2c_address_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 data;
	s8 err;

	err = SMI_CALL_API(set_mag_manual_enable)(1);
	err += SMI_CALL_API(get_i2c_device_addr)(&data);
	err += SMI_CALL_API(set_mag_manual_enable)(0);

	if (err < 0)
		return err;
	return snprintf(buf, 16, "0x%x\n", data);
}

static ssize_t smi130_mag_i2c_address_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err += SMI_CALL_API(set_mag_manual_enable)(1);
	if (!err)
		err += SMI_CALL_API(set_i2c_device_addr)((unsigned char)data);
	err += SMI_CALL_API(set_mag_manual_enable)(0);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t smi130_mag_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	struct smi130_mag_xyz_s32_t data;
	int err;
	/* raw data with compensation */
#if defined(SMI130_AKM09912_SUPPORT)
	err = smi130_bosch_akm09912_compensate_xyz(&data);
#else
	err = smi130_bmm150_mag_compensate_xyz(&data);
#endif

	if (err < 0) {
		memset(&data, 0, sizeof(data));
		dev_err(client_data->dev, "mag not ready!\n");
	}
	return snprintf(buf, 48, "%hd %hd %hd\n", data.x,
				data.y, data.z);
}
static ssize_t smi130_mag_offset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char mag_offset;
	err = SMI_CALL_API(get_mag_offset)(&mag_offset);
	if (err)
		return err;

	return snprintf(buf, 16, "%d\n", mag_offset);

}

static ssize_t smi130_mag_offset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err += SMI_CALL_API(set_mag_manual_enable)(1);
	if (err == 0)
		err += SMI_CALL_API(set_mag_offset)((unsigned char)data);
	err += SMI_CALL_API(set_mag_manual_enable)(0);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t smi130_mag_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s8 err = 0;
	u8 mag_chipid;

	err = smi130_set_mag_manual_enable(0x01);
	/* read mag chip_id value */
#if defined(SMI130_AKM09912_SUPPORT)
	err += smi130_set_mag_read_addr(AKM09912_CHIP_ID_REG);
		/* 0x04 is mag_x lsb register */
	err += smi130_read_reg(SMI130_USER_DATA_0_MAG_X_LSB__REG,
							&mag_chipid, 1);

	/* Must add this commands to re-set data register addr of mag sensor */
	err += smi130_set_mag_read_addr(AKM_DATA_REGISTER);
#else
	err += smi130_set_mag_read_addr(SMI130_BMM150_CHIP_ID);
	/* 0x04 is mag_x lsb register */
	err += smi130_read_reg(SMI130_USER_DATA_0_MAG_X_LSB__REG,
							&mag_chipid, 1);

	/* Must add this commands to re-set data register addr of mag sensor */
	/* 0x42 is  bmm150 data register address */
	err += smi130_set_mag_read_addr(SMI130_BMM150_DATA_REG);
#endif

	err += smi130_set_mag_manual_enable(0x00);

	if (err)
		return err;

	return snprintf(buf, 16, "%x\n", mag_chipid);

}

static ssize_t smi130_mag_chip_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 mag_chipid = 0;
#if defined(SMI130_AKM09912_SUPPORT)
	mag_chipid = 15;
#else
	mag_chipid = 150;
#endif
	return snprintf(buf, 16, "%d\n", mag_chipid);
}

struct smi130_mag_xyz_s32_t mag_compensate;
static ssize_t smi130_mag_compensate_xyz_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	memcpy(buf, &mag_compensate, sizeof(mag_compensate));
	return sizeof(mag_compensate);
}
static ssize_t smi130_mag_compensate_xyz_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct smi130_mag_xyzr_t mag_raw;
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
	smi130_bmm150_mag_compensate_xyz_raw(
	&mag_compensate, mag_raw);
	return count;
}

#endif

#if defined(SMI130_ENABLE_INT1) || defined(SMI130_ENABLE_INT2)
static ssize_t smi_enable_int_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int interrupt_type, value;

	sscanf(buf, "%3d %3d", &interrupt_type, &value);

	if (interrupt_type < 0 || interrupt_type > 16)
		return -EINVAL;

	if (interrupt_type <= SMI_FLAT_INT) {
		if (SMI_CALL_API(set_intr_enable_0)
				(smi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	} else if (interrupt_type <= SMI_FWM_INT) {
		if (SMI_CALL_API(set_intr_enable_1)
			(smi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	} else {
		if (SMI_CALL_API(set_intr_enable_2)
			(smi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	}

	return count;
}

#endif

static ssize_t smi130_show_reg_sel(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	return snprintf(buf, 64, "reg=0X%02X, len=%d\n",
		client_data->reg_sel, client_data->reg_len);
}

static ssize_t smi130_store_reg_sel(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
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

static ssize_t smi130_show_reg_val(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);

	ssize_t ret;
	u8 reg_data[128] = {0}, i;
	int pos;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	ret = smi_burst_read_wrapper(client_data->device.dev_addr,
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

static ssize_t smi130_store_reg_val(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	u8 reg_data[32] = {0};

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

	ret = SMI_CALL_API(write_reg)(
		client_data->reg_sel,
		reg_data, client_data->reg_len);
	if (ret < 0) {
		dev_err(client_data->dev, "Reg op failed");
		return ret;
	}

	return count;
}

static ssize_t smi130_driver_version_show(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct smi_client_data *client_data = input_get_drvdata(input);
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
		smi130_chip_id_show, NULL);
static DEVICE_ATTR(err_st, S_IRUGO,
		smi130_err_st_show, NULL);
static DEVICE_ATTR(sensor_time, S_IRUGO,
		smi130_sensor_time_show, NULL);

static DEVICE_ATTR(selftest, S_IRUGO | S_IWUSR,
		smi130_selftest_show, smi130_selftest_store);
static DEVICE_ATTR(fifo_flush, S_IRUGO | S_IWUSR,
		NULL, smi130_fifo_flush_store);
static DEVICE_ATTR(fifo_bytecount, S_IRUGO | S_IWUSR,
		smi130_fifo_bytecount_show, smi130_fifo_bytecount_store);
static DEVICE_ATTR(fifo_data_sel, S_IRUGO | S_IWUSR,
		smi130_fifo_data_sel_show, smi130_fifo_data_sel_store);
static DEVICE_ATTR(fifo_data_frame, S_IRUGO,
		smi130_fifo_data_out_frame_show, NULL);

static DEVICE_ATTR(fifo_watermark, S_IRUGO | S_IWUSR,
		smi130_fifo_watermark_show, smi130_fifo_watermark_store);

static DEVICE_ATTR(fifo_header_en, S_IRUGO | S_IWUSR,
		smi130_fifo_header_en_show, smi130_fifo_header_en_store);
static DEVICE_ATTR(fifo_time_en, S_IRUGO | S_IWUSR,
		smi130_fifo_time_en_show, smi130_fifo_time_en_store);
static DEVICE_ATTR(fifo_int_tag_en, S_IRUGO | S_IWUSR,
		smi130_fifo_int_tag_en_show, smi130_fifo_int_tag_en_store);

static DEVICE_ATTR(temperature, S_IRUGO,
		smi130_temperature_show, NULL);
static DEVICE_ATTR(place, S_IRUGO,
		smi130_place_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO | S_IWUSR,
		smi130_delay_show, smi130_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR,
		smi130_enable_show, smi130_enable_store);
static DEVICE_ATTR(acc_range, S_IRUGO | S_IWUSR,
		smi130_acc_range_show, smi130_acc_range_store);
static DEVICE_ATTR(acc_odr, S_IRUGO | S_IWUSR,
		smi130_acc_odr_show, smi130_acc_odr_store);
static DEVICE_ATTR(acc_op_mode, S_IRUGO | S_IWUSR,
		smi130_acc_op_mode_show, smi130_acc_op_mode_store);
static DEVICE_ATTR(acc_value, S_IRUGO,
		smi130_acc_value_show, NULL);
static DEVICE_ATTR(acc_fast_calibration_x, S_IRUGO | S_IWUSR,
		smi130_acc_fast_calibration_x_show,
		smi130_acc_fast_calibration_x_store);
static DEVICE_ATTR(acc_fast_calibration_y, S_IRUGO | S_IWUSR,
		smi130_acc_fast_calibration_y_show,
		smi130_acc_fast_calibration_y_store);
static DEVICE_ATTR(acc_fast_calibration_z, S_IRUGO | S_IWUSR,
		smi130_acc_fast_calibration_z_show,
		smi130_acc_fast_calibration_z_store);
static DEVICE_ATTR(acc_offset_x, S_IRUGO | S_IWUSR,
		smi130_acc_offset_x_show,
		smi130_acc_offset_x_store);
static DEVICE_ATTR(acc_offset_y, S_IRUGO | S_IWUSR,
		smi130_acc_offset_y_show,
		smi130_acc_offset_y_store);
static DEVICE_ATTR(acc_offset_z, S_IRUGO | S_IWUSR,
		smi130_acc_offset_z_show,
		smi130_acc_offset_z_store);
static DEVICE_ATTR(test, S_IRUGO,
		smi130_test_show, NULL);
static DEVICE_ATTR(stc_enable, S_IRUGO | S_IWUSR,
		smi130_step_counter_enable_show,
		smi130_step_counter_enable_store);
static DEVICE_ATTR(stc_mode, S_IRUGO | S_IWUSR,
		NULL, smi130_step_counter_mode_store);
static DEVICE_ATTR(stc_clc, S_IRUGO | S_IWUSR,
		NULL, smi130_step_counter_clc_store);
static DEVICE_ATTR(stc_value, S_IRUGO,
		smi130_step_counter_value_show, NULL);
static DEVICE_ATTR(reg_sel, S_IRUGO | S_IWUSR,
		smi130_show_reg_sel, smi130_store_reg_sel);
static DEVICE_ATTR(reg_val, S_IRUGO | S_IWUSR,
		smi130_show_reg_val, smi130_store_reg_val);
static DEVICE_ATTR(driver_version, S_IRUGO,
		smi130_driver_version_show, NULL);
/* gyro part */
static DEVICE_ATTR(gyro_op_mode, S_IRUGO | S_IWUSR,
		smi130_gyro_op_mode_show, smi130_gyro_op_mode_store);
static DEVICE_ATTR(gyro_value, S_IRUGO,
		smi130_gyro_value_show, NULL);
static DEVICE_ATTR(gyro_range, S_IRUGO | S_IWUSR,
		smi130_gyro_range_show, smi130_gyro_range_store);
static DEVICE_ATTR(gyro_odr, S_IRUGO | S_IWUSR,
		smi130_gyro_odr_show, smi130_gyro_odr_store);
static DEVICE_ATTR(gyro_fast_calibration_en, S_IRUGO | S_IWUSR,
smi130_gyro_fast_calibration_en_show, smi130_gyro_fast_calibration_en_store);
static DEVICE_ATTR(gyro_offset_x, S_IRUGO | S_IWUSR,
smi130_gyro_offset_x_show, smi130_gyro_offset_x_store);
static DEVICE_ATTR(gyro_offset_y, S_IRUGO | S_IWUSR,
smi130_gyro_offset_y_show, smi130_gyro_offset_y_store);
static DEVICE_ATTR(gyro_offset_z, S_IRUGO | S_IWUSR,
smi130_gyro_offset_z_show, smi130_gyro_offset_z_store);

#ifdef SMI130_MAG_INTERFACE_SUPPORT
static DEVICE_ATTR(mag_op_mode, S_IRUGO | S_IWUSR,
		smi130_mag_op_mode_show, smi130_mag_op_mode_store);
static DEVICE_ATTR(mag_odr, S_IRUGO | S_IWUSR,
		smi130_mag_odr_show, smi130_mag_odr_store);
static DEVICE_ATTR(mag_i2c_addr, S_IRUGO | S_IWUSR,
		smi130_mag_i2c_address_show, smi130_mag_i2c_address_store);
static DEVICE_ATTR(mag_value, S_IRUGO,
		smi130_mag_value_show, NULL);
static DEVICE_ATTR(mag_offset, S_IRUGO | S_IWUSR,
		smi130_mag_offset_show, smi130_mag_offset_store);
static DEVICE_ATTR(mag_chip_id, S_IRUGO,
		smi130_mag_chip_id_show, NULL);
static DEVICE_ATTR(mag_chip_name, S_IRUGO,
		smi130_mag_chip_name_show, NULL);
static DEVICE_ATTR(mag_compensate, S_IRUGO | S_IWUSR,
		smi130_mag_compensate_xyz_show,
		smi130_mag_compensate_xyz_store);
#endif


#if defined(SMI130_ENABLE_INT1) || defined(SMI130_ENABLE_INT2)
static DEVICE_ATTR(enable_int, S_IRUGO | S_IWUSR,
		NULL, smi_enable_int_store);
static DEVICE_ATTR(anymot_duration, S_IRUGO | S_IWUSR,
		smi130_anymot_duration_show, smi130_anymot_duration_store);
static DEVICE_ATTR(anymot_threshold, S_IRUGO | S_IWUSR,
		smi130_anymot_threshold_show, smi130_anymot_threshold_store);
static DEVICE_ATTR(std_stu, S_IRUGO,
		smi130_step_detector_status_show, NULL);
static DEVICE_ATTR(std_en, S_IRUGO | S_IWUSR,
		smi130_step_detector_enable_show,
		smi130_step_detector_enable_store);
static DEVICE_ATTR(sig_en, S_IRUGO | S_IWUSR,
		smi130_signification_motion_enable_show,
		smi130_signification_motion_enable_store);

#endif



static DEVICE_ATTR(smi_value, S_IRUGO,
		smi130_smi_value_show, NULL);


static struct attribute *smi130_attributes[] = {
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

#ifdef SMI130_MAG_INTERFACE_SUPPORT
	&dev_attr_mag_chip_id.attr,
	&dev_attr_mag_op_mode.attr,
	&dev_attr_mag_odr.attr,
	&dev_attr_mag_i2c_addr.attr,
	&dev_attr_mag_chip_name.attr,
	&dev_attr_mag_value.attr,
	&dev_attr_mag_offset.attr,
	&dev_attr_mag_compensate.attr,
#endif

#if defined(SMI130_ENABLE_INT1) || defined(SMI130_ENABLE_INT2)
	&dev_attr_enable_int.attr,

	&dev_attr_anymot_duration.attr,
	&dev_attr_anymot_threshold.attr,
	&dev_attr_std_stu.attr,
	&dev_attr_std_en.attr,
	&dev_attr_sig_en.attr,

#endif
	&dev_attr_reg_sel.attr,
	&dev_attr_reg_val.attr,
	&dev_attr_smi_value.attr,
	NULL
};

static struct attribute_group smi130_attribute_group = {
	.attrs = smi130_attributes
};

#if defined(SMI130_ENABLE_INT1) || defined(SMI130_ENABLE_INT2)
static void smi_slope_interrupt_handle(struct smi_client_data *client_data)
{
	/* anym_first[0..2]: x, y, z */
	u8 anym_first[3] = {0};
	u8 status2;
	u8 anym_sign;
	u8 i = 0;

	client_data->device.bus_read(client_data->device.dev_addr,
				SMI130_USER_INTR_STAT_2_ADDR, &status2, 1);
	anym_first[0] = SMI130_GET_BITSLICE(status2,
				SMI130_USER_INTR_STAT_2_ANY_MOTION_FIRST_X);
	anym_first[1] = SMI130_GET_BITSLICE(status2,
				SMI130_USER_INTR_STAT_2_ANY_MOTION_FIRST_Y);
	anym_first[2] = SMI130_GET_BITSLICE(status2,
				SMI130_USER_INTR_STAT_2_ANY_MOTION_FIRST_Z);
	anym_sign = SMI130_GET_BITSLICE(status2,
				SMI130_USER_INTR_STAT_2_ANY_MOTION_SIGN);

	for (i = 0; i < 3; i++) {
		if (anym_first[i]) {
			/*1: negative*/
			if (anym_sign)
				dev_notice(client_data->dev,
				"Anymotion interrupt happend!"
				"%s axis, negative sign\n", smi_axis_name[i]);
			else
				dev_notice(client_data->dev,
				"Anymotion interrupt happend!"
				"%s axis, postive sign\n", smi_axis_name[i]);
		}
	}


}

static void smi_fifo_watermark_interrupt_handle
				(struct smi_client_data *client_data)
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
	smi_fifo_frame_bytes_extend_calc(client_data, &fifo_frmbytes_ext);

	if (client_data->pw.acc_pm == 2 && client_data->pw.gyro_pm == 2
					&& client_data->pw.mag_pm == 2)
		printk(KERN_INFO "pw_acc: %d, pw_gyro: %d\n",
			client_data->pw.acc_pm, client_data->pw.gyro_pm);
	if (!client_data->fifo_data_sel)
		printk(KERN_INFO "no selsect sensor fifo, fifo_data_sel:%d\n",
						client_data->fifo_data_sel);

	err = SMI_CALL_API(fifo_length)(&fifo_len0);
	client_data->fifo_bytecount = fifo_len0;

	if (client_data->fifo_bytecount == 0 || err)
		return;

	if (client_data->fifo_bytecount + fifo_frmbytes_ext > FIFO_DATA_BUFSIZE)
		client_data->fifo_bytecount = FIFO_DATA_BUFSIZE;
	/* need give attention for the time of burst read*/
	if (!err) {
		err = smi_burst_read_wrapper(client_data->device.dev_addr,
			SMI130_USER_FIFO_DATA__REG, fifo_data,
			client_data->fifo_bytecount + fifo_frmbytes_ext);
	} else
		dev_err(client_data->dev, "read fifo leght err");

	if (err)
		dev_err(client_data->dev, "brust read fifo err\n");
	/*err = smi_fifo_analysis_handle(client_data, fifo_data,
			client_data->fifo_bytecount + 20, fifo_out_data);*/
	if (fifo_data != NULL) {
		kfree(fifo_data);
		fifo_data = NULL;
	}

}
static void smi_data_ready_interrupt_handle(
	struct smi_client_data *client_data, uint8_t status)
{
	uint8_t data12[12] = {0};
	struct smi130_accel_t accel;
	struct smi130_gyro_t gyro;
	struct timespec ts;
	client_data->device.bus_read(client_data->device.dev_addr,
	SMI130_USER_DATA_8_ADDR, data12, 12);
	if (status & 0x80)
	{
		/*report acc data*/
		/* Data X */
		accel.x = (s16)((((s32)((s8)data12[7])) << SMI130_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[6]));
		/* Data Y */
		accel.y = (s16)((((s32)((s8)data12[9])) << SMI130_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[8]));
		/* Data Z */
		accel.z = (s16)((((s32)((s8)data12[11]))<< SMI130_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[10]));
		ts = ns_to_timespec(client_data->timestamp);
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
		gyro.x = (s16)((((s32)((s8)data12[1])) << SMI130_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[0]));
		/* Data Y */
		gyro.y = (s16)((((s32)((s8)data12[3])) << SMI130_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[2]));
		/* Data Z */
		gyro.z = (s16)((((s32)((s8)data12[5]))<< SMI130_SHIFT_BIT_POSITION_BY_08_BITS) | (data12[4]));
		ts = ns_to_timespec(client_data->timestamp);
		input_event(client_data->gyro_input, EV_MSC, 6, ts.tv_sec);
		input_event(client_data->gyro_input, EV_MSC, 6, ts.tv_nsec);
		input_event(client_data->gyro_input, EV_MSC, MSC_GESTURE, gyro.x);
		input_event(client_data->gyro_input, EV_MSC, MSC_RAW, gyro.y);
		input_event(client_data->gyro_input, EV_MSC, MSC_SCAN, gyro.z);
		input_sync(client_data->gyro_input);
	}
}

static void smi_signification_motion_interrupt_handle(
		struct smi_client_data *client_data)
{
	printk(KERN_INFO "smi_signification_motion_interrupt_handle\n");
	input_event(client_data->input, EV_MSC, INPUT_EVENT_SGM, 1);
/*input_report_rel(client_data->input,INPUT_EVENT_SGM,1);*/
	input_sync(client_data->input);
	smi130_set_command_register(CMD_RESET_INT_ENGINE);

}
static void smi_stepdetector_interrupt_handle(
	struct smi_client_data *client_data)
{
	u8 current_step_dector_st = 0;
	client_data->pedo_data.wkar_step_detector_status++;
	current_step_dector_st =
		client_data->pedo_data.wkar_step_detector_status;
	client_data->std = ((current_step_dector_st == 1) ? 0 : 1);

	input_event(client_data->input, EV_MSC, INPUT_EVENT_STEP_DETECTOR, 1);
	input_sync(client_data->input);
}

static void smi_irq_work_func(struct work_struct *work)
{
	struct smi_client_data *client_data =
		container_of((struct work_struct *)work,
			struct smi_client_data, irq_work);

	unsigned char int_status[4] = {0, 0, 0, 0};
	uint8_t status = 0;

	//client_data->device.bus_read(client_data->device.dev_addr,
	//			SMI130_USER_INTR_STAT_0_ADDR, int_status, 4);
	client_data->device.bus_read(client_data->device.dev_addr,
	SMI130_USER_STAT_ADDR, &status, 1);
	printk("status = 0x%x", status);
	if (SMI130_GET_BITSLICE(int_status[0],
					SMI130_USER_INTR_STAT_0_ANY_MOTION))
		smi_slope_interrupt_handle(client_data);

	if (SMI130_GET_BITSLICE(int_status[0],
			SMI130_USER_INTR_STAT_0_STEP_INTR))
		smi_stepdetector_interrupt_handle(client_data);
	if (SMI130_GET_BITSLICE(int_status[1],
			SMI130_USER_INTR_STAT_1_FIFO_WM_INTR))
		smi_fifo_watermark_interrupt_handle(client_data);
	if ((status & 0x80) || (status & 0x40))
		smi_data_ready_interrupt_handle(client_data, status);
	/* Clear ALL inputerrupt status after handler sig mition*/
	/* Put this commads intot the last one*/
	if (SMI130_GET_BITSLICE(int_status[0],
		SMI130_USER_INTR_STAT_0_SIGNIFICANT_INTR))
		smi_signification_motion_interrupt_handle(client_data);

}

static void smi130_delay_sigmo_work_func(struct work_struct *work)
{
	struct smi_client_data *client_data =
	container_of(work, struct smi_client_data,
	delay_work_sig.work);
	unsigned char int_status[4] = {0, 0, 0, 0};

	client_data->device.bus_read(client_data->device.dev_addr,
				SMI130_USER_INTR_STAT_0_ADDR, int_status, 4);
	if (SMI130_GET_BITSLICE(int_status[0],
		SMI130_USER_INTR_STAT_0_SIGNIFICANT_INTR))
		smi_signification_motion_interrupt_handle(client_data);
}

static irqreturn_t smi_irq_handler(int irq, void *handle)
{
	struct smi_client_data *client_data = handle;
	int in_suspend_copy;
	in_suspend_copy = atomic_read(&client_data->in_suspend);

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
#endif /* defined(SMI_ENABLE_INT1)||defined(SMI_ENABLE_INT2) */

static int smi_restore_hw_cfg(struct smi_client_data *client)
{
	int err = 0;

	if ((client->fifo_data_sel) & (1 << SMI_ACC_SENSOR)) {
		err += SMI_CALL_API(set_accel_range)(client->range.acc_range);
		err += SMI_CALL_API(set_accel_output_data_rate)
				(client->odr.acc_odr);
		err += SMI_CALL_API(set_fifo_accel_enable)(1);
	}
	if ((client->fifo_data_sel) & (1 << SMI_GYRO_SENSOR)) {
		err += SMI_CALL_API(set_gyro_range)(client->range.gyro_range);
		err += SMI_CALL_API(set_gyro_output_data_rate)
				(client->odr.gyro_odr);
		err += SMI_CALL_API(set_fifo_gyro_enable)(1);
	}
	if ((client->fifo_data_sel) & (1 << SMI_MAG_SENSOR)) {
		err += SMI_CALL_API(set_mag_output_data_rate)
				(client->odr.mag_odr);
		err += SMI_CALL_API(set_fifo_mag_enable)(1);
	}
	err += SMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);

	mutex_lock(&client->mutex_op_mode);
	if (client->pw.acc_pm != SMI_ACC_PM_SUSPEND) {
		err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_acc_arr[SMI_ACC_PM_NORMAL]);
		smi_delay(3);
	}
	mutex_unlock(&client->mutex_op_mode);

	mutex_lock(&client->mutex_op_mode);
	if (client->pw.gyro_pm != SMI_GYRO_PM_SUSPEND) {
		err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_NORMAL]);
		smi_delay(3);
	}
	mutex_unlock(&client->mutex_op_mode);

	mutex_lock(&client->mutex_op_mode);

	if (client->pw.mag_pm != SMI_MAG_PM_SUSPEND) {
#ifdef SMI130_AKM09912_SUPPORT
		err += smi130_set_bosch_akm_and_secondary_if_powermode
					(SMI130_MAG_FORCE_MODE);
#else
		err += smi130_set_bmm150_mag_and_secondary_if_power_mode
					(SMI130_MAG_FORCE_MODE);
#endif
		smi_delay(3);
	}
	mutex_unlock(&client->mutex_op_mode);

	return err;
}

#if defined(CONFIG_USE_QUALCOMM_HAL)
static void smi130_accel_work_fn(struct work_struct *work)
{
	struct smi_client_data *sensor;
	ktime_t timestamp;
	struct smi130_accel_t data;
	int err;
	sensor = container_of((struct delayed_work *)work,
				struct smi_client_data, accel_poll_work);
	timestamp = ktime_get();
	err = SMI_CALL_API(read_accel_xyz)(&data);
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
static void smi130_gyro_work_fn(struct work_struct *work)
{
	struct smi_client_data *sensor;
	ktime_t timestamp;
	struct smi130_gyro_t data;
	int err;
	sensor = container_of((struct delayed_work *)work,
				struct smi_client_data, gyro_poll_work);
	timestamp = ktime_get();
	err = SMI_CALL_API(read_gyro_xyz)(&data);
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
static int smi130_set_gyro_op_mode(struct smi_client_data *client_data,
							unsigned long op_mode)
{
	int err = 0;
	mutex_lock(&client_data->mutex_op_mode);
	if (op_mode < SMI_GYRO_PM_MAX) {
		switch (op_mode) {
		case SMI_GYRO_PM_NORMAL:
			err = SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_NORMAL]);
			client_data->pw.gyro_pm = SMI_GYRO_PM_NORMAL;
			smi_delay(60);
			break;
		case SMI_GYRO_PM_FAST_START:
			err = SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_FAST_START]);
			client_data->pw.gyro_pm = SMI_GYRO_PM_FAST_START;
			smi_delay(60);
			break;
		case SMI_GYRO_PM_SUSPEND:
			err = SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_SUSPEND]);
			client_data->pw.gyro_pm = SMI_GYRO_PM_SUSPEND;
			smi_delay(60);
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
static int smi130_accel_set_enable(
	struct smi_client_data *client_data, bool enable)
{
	int ret = 0;
	dev_notice(client_data->dev,
		"smi130_accel_set_enable enable=%d\n", enable);
	if (enable) {
		ret = smi130_set_acc_op_mode(client_data, 0);
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
		ret = smi130_set_acc_op_mode(client_data, 2);
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
static int smi130_accel_set_poll_delay(struct smi_client_data *client_data,
					unsigned long delay)
{
	dev_info(client_data->dev,
		"smi130_accel_set_poll_delay delay_ms=%ld\n", delay);
	if (delay < SMI130_ACCEL_MIN_POLL_INTERVAL_MS)
		delay = SMI130_ACCEL_MIN_POLL_INTERVAL_MS;
	if (delay > SMI130_ACCEL_MAX_POLL_INTERVAL_MS)
		delay = SMI130_ACCEL_MAX_POLL_INTERVAL_MS;
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
static int smi130_gyro_set_enable(
	struct smi_client_data *client_data, bool enable)
{
	int ret = 0;
	dev_notice(client_data->dev,
		"smi130_gyro_set_enable enable=%d\n", enable);
	if (enable) {
		ret = smi130_set_gyro_op_mode(client_data, 0);
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
		ret = smi130_set_gyro_op_mode(client_data, 2);
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
static int smi130_gyro_set_poll_delay(struct smi_client_data *client_data,
					unsigned long delay)
{
	dev_info(client_data->dev,
		"smi130_accel_set_poll_delay delay_ms=%ld\n", delay);
	if (delay < SMI130_GYRO_MIN_POLL_INTERVAL_MS)
		delay = SMI130_GYRO_MIN_POLL_INTERVAL_MS;
	if (delay > SMI130_GYRO_MAX_POLL_INTERVAL_MS)
		delay = SMI130_GYRO_MAX_POLL_INTERVAL_MS;
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
static int smi130_accel_cdev_enable(struct sensors_classdev *sensors_cdev,
			unsigned int enable)
{
	struct smi_client_data *sensor = container_of(sensors_cdev,
			struct smi_client_data, accel_cdev);
	return smi130_accel_set_enable(sensor, enable);
}
static int smi130_accel_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
			unsigned int delay_ms)
{
	struct smi_client_data *sensor = container_of(sensors_cdev,
			struct smi_client_data, accel_cdev);

	return smi130_accel_set_poll_delay(sensor, delay_ms);
}

static int smi130_gyro_cdev_enable(struct sensors_classdev *sensors_cdev,
			unsigned int enable)
{
	struct smi_client_data *sensor = container_of(sensors_cdev,
			struct smi_client_data, gyro_cdev);

	return smi130_gyro_set_enable(sensor, enable);
}

static int smi130_gyro_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
			unsigned int delay_ms)
{
	struct smi_client_data *sensor = container_of(sensors_cdev,
			struct smi_client_data, gyro_cdev);

	return	smi130_gyro_set_poll_delay(sensor, delay_ms);
}
#endif

int smi_probe(struct smi_client_data *client_data, struct device *dev)
{
	int err = 0;
#ifdef SMI130_MAG_INTERFACE_SUPPORT
	u8 mag_dev_addr;
	u8 mag_urst_len;
	u8 mag_op_mode;
#endif
	/* check chip id */
	err = smi_check_chip_id(client_data);
	if (err)
		goto exit_err_clean;

	dev_set_drvdata(dev, client_data);
	client_data->dev = dev;

	mutex_init(&client_data->mutex_enable);
	mutex_init(&client_data->mutex_op_mode);

	/* input device init */
	err = smi_input_init(client_data);
	if (err < 0)
		goto exit_err_clean;

	/* sysfs node creation */
	err = sysfs_create_group(&client_data->input->dev.kobj,
			&smi130_attribute_group);

	if (err < 0)
		goto exit_err_sysfs;

	if (NULL != dev->platform_data) {
		client_data->bosch_pd = kzalloc(sizeof(*client_data->bosch_pd),
				GFP_KERNEL);

		if (NULL != client_data->bosch_pd) {
			memcpy(client_data->bosch_pd, dev->platform_data,
					sizeof(*client_data->bosch_pd));
			dev_notice(dev, "%s sensor driver set place: p%d\n",
					client_data->bosch_pd->name,
					client_data->bosch_pd->place);
		}
	}

	if (NULL != client_data->bosch_pd) {
			memcpy(client_data->bosch_pd, dev->platform_data,
					sizeof(*client_data->bosch_pd));
			dev_notice(dev, "%s sensor driver set place: p%d\n",
					client_data->bosch_pd->name,
					client_data->bosch_pd->place);
		}


	/* workqueue init */
	INIT_DELAYED_WORK(&client_data->work, smi_work_func);
	atomic_set(&client_data->delay, SMI_DELAY_DEFAULT);
	atomic_set(&client_data->wkqueue_en, 0);

	/* h/w init */
	client_data->device.delay_msec = smi_delay;
	err = SMI_CALL_API(init)(&client_data->device);

	smi_dump_reg(client_data);

	/*power on detected*/
	/*or softrest(cmd 0xB6) */
	/*fatal err check*/
	/*soft reset*/
	err += SMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	smi_delay(3);
	if (err)
		dev_err(dev, "Failed soft reset, er=%d", err);
	/*usr data config page*/
	err += SMI_CALL_API(set_target_page)(USER_DAT_CFG_PAGE);
	if (err)
		dev_err(dev, "Failed cffg page, er=%d", err);
	err += smi_get_err_status(client_data);
	if (err) {
		dev_err(dev, "Failed to smi16x init!err_st=0x%x\n",
				client_data->err_st.err_st_all);
		goto exit_err_sysfs;
	}

#ifdef SMI130_MAG_INTERFACE_SUPPORT
	err += smi130_set_command_register(MAG_MODE_NORMAL);
	smi_delay(2);
	err += smi130_get_mag_power_mode_stat(&mag_op_mode);
	smi_delay(2);
	err += SMI_CALL_API(get_i2c_device_addr)(&mag_dev_addr);
	smi_delay(2);
#if defined(SMI130_AKM09912_SUPPORT)
	err += SMI_CALL_API(set_i2c_device_addr)(SMI130_AKM09912_I2C_ADDRESS);
	smi130_bosch_akm_mag_interface_init(SMI130_AKM09912_I2C_ADDRESS);
#else
	err += SMI_CALL_API(set_i2c_device_addr)(
		SMI130_AUX_BMM150_I2C_ADDRESS);
	smi130_bmm150_mag_interface_init();
#endif

	err += smi130_set_mag_burst(3);
	err += smi130_get_mag_burst(&mag_urst_len);
	if (err)
		dev_err(client_data->dev, "Failed cffg mag, er=%d", err);
	dev_info(client_data->dev,
		"SMI130 mag_urst_len:%d, mag_add:0x%x, mag_op_mode:%d\n",
		mag_urst_len, mag_dev_addr, mag_op_mode);
#endif
	if (err < 0)
		goto exit_err_sysfs;


#if defined(SMI130_ENABLE_INT1) || defined(SMI130_ENABLE_INT2)
		/*wake_lock_init(&client_data->wakelock,
			WAKE_LOCK_SUSPEND, "smi130");*/
		client_data->gpio_pin = of_get_named_gpio_flags(dev->of_node,
					"smi,gpio_irq", 0, NULL);
		dev_info(client_data->dev, "SMI130 qpio number:%d\n",
					client_data->gpio_pin);
		err += gpio_request_one(client_data->gpio_pin,
					GPIOF_IN, "smi130_int");
		err += gpio_direction_input(client_data->gpio_pin);
		client_data->IRQ = gpio_to_irq(client_data->gpio_pin);
		if (err) {
			dev_err(client_data->dev,
				"can not request gpio to irq number\n");
			client_data->gpio_pin = 0;
		}
		INIT_DELAYED_WORK(&client_data->delay_work_sig,
			smi130_delay_sigmo_work_func);
#ifdef SMI130_ENABLE_INT1
		/* maps interrupt to INT1/InT2 pin */
		SMI_CALL_API(set_intr_any_motion)(SMI_INT0, ENABLE);
		SMI_CALL_API(set_intr_fifo_wm)(SMI_INT0, ENABLE);
		SMI_CALL_API(set_intr_data_rdy)(SMI_INT0, ENABLE);

		/*Set interrupt trige level way */
		SMI_CALL_API(set_intr_edge_ctrl)(SMI_INT0, SMI_INT_LEVEL);
		smi130_set_intr_level(SMI_INT0, 1);
		/*set interrupt latch temporary, 5 ms*/
		/*smi130_set_latch_int(5);*/

		SMI_CALL_API(set_output_enable)(
		SMI130_INTR1_OUTPUT_ENABLE, ENABLE);
		sigmotion_init_interrupts(SMI130_MAP_INTR1);
		SMI_CALL_API(map_step_detector_intr)(SMI130_MAP_INTR1);
		/*close step_detector in init function*/
		SMI_CALL_API(set_step_detector_enable)(0);
#endif

#ifdef SMI130_ENABLE_INT2
		/* maps interrupt to INT1/InT2 pin */
		SMI_CALL_API(set_intr_any_motion)(SMI_INT1, ENABLE);
		SMI_CALL_API(set_intr_fifo_wm)(SMI_INT1, ENABLE);
		SMI_CALL_API(set_intr_data_rdy)(SMI_INT1, ENABLE);

		/*Set interrupt trige level way */
		SMI_CALL_API(set_intr_edge_ctrl)(SMI_INT1, SMI_INT_LEVEL);
		smi130_set_intr_level(SMI_INT1, 1);
		/*set interrupt latch temporary, 5 ms*/
		/*smi130_set_latch_int(5);*/

		SMI_CALL_API(set_output_enable)(
		SMI130_INTR2_OUTPUT_ENABLE, ENABLE);
		sigmotion_init_interrupts(SMI130_MAP_INTR2);
		SMI_CALL_API(map_step_detector_intr)(SMI130_MAP_INTR2);
		/*close step_detector in init function*/
		SMI_CALL_API(set_step_detector_enable)(0);
#endif
		err = request_irq(client_data->IRQ, smi_irq_handler,
				IRQF_TRIGGER_RISING, "smi130", client_data);
		if (err)
			dev_err(client_data->dev, "could not request irq\n");

		INIT_WORK(&client_data->irq_work, smi_irq_work_func);
#endif

	client_data->selftest = 0;

	client_data->fifo_data_sel = 0;
	#if defined(CONFIG_USE_QUALCOMM_HAL)
	SMI_CALL_API(set_accel_output_data_rate)(9);/*defalut odr 200HZ*/
	SMI_CALL_API(set_gyro_output_data_rate)(9);/*defalut odr 200HZ*/
	#endif
	SMI_CALL_API(get_accel_output_data_rate)(&client_data->odr.acc_odr);
	SMI_CALL_API(get_gyro_output_data_rate)(&client_data->odr.gyro_odr);
	SMI_CALL_API(get_mag_output_data_rate)(&client_data->odr.mag_odr);
	SMI_CALL_API(set_fifo_time_enable)(1);
	SMI_CALL_API(get_accel_range)(&client_data->range.acc_range);
	SMI_CALL_API(get_gyro_range)(&client_data->range.gyro_range);
	/* now it's power on which is considered as resuming from suspend */
	
	/* gyro input device init */
	err = smi_gyro_input_init(client_data);
	#if defined(CONFIG_USE_QUALCOMM_HAL)
	/* gyro input device init */
	err = smi_gyro_input_init(client_data);
	if (err < 0)
		goto exit_err_clean;
	client_data->accel_poll_ms = SMI130_ACCEL_DEFAULT_POLL_INTERVAL_MS;
	client_data->gyro_poll_ms = SMI130_GYRO_DEFAULT_POLL_INTERVAL_MS;
	client_data->data_wq = create_freezable_workqueue("smi130_data_work");
	if (!client_data->data_wq) {
		dev_err(dev, "Cannot create workqueue!\n");
		goto exit_err_clean;
	}
	INIT_DELAYED_WORK(&client_data->accel_poll_work,
		smi130_accel_work_fn);
	client_data->accel_cdev = smi130_accel_cdev;
	client_data->accel_cdev.delay_msec = client_data->accel_poll_ms;
	client_data->accel_cdev.sensors_enable = smi130_accel_cdev_enable;
	client_data->accel_cdev.sensors_poll_delay =
	smi130_accel_cdev_poll_delay;
	err = sensors_classdev_register(dev, &client_data->accel_cdev);
	if (err) {
		dev_err(dev,
			"create accel class device file failed!\n");
		goto exit_err_clean;
	}
	INIT_DELAYED_WORK(&client_data->gyro_poll_work, smi130_gyro_work_fn);
	client_data->gyro_cdev = smi130_gyro_cdev;
	client_data->gyro_cdev.delay_msec = client_data->gyro_poll_ms;
	client_data->gyro_cdev.sensors_enable = smi130_gyro_cdev_enable;
	client_data->gyro_cdev.sensors_poll_delay = smi130_gyro_cdev_poll_delay;
	err = sensors_classdev_register(dev, &client_data->gyro_cdev);
	if (err) {
		dev_err(dev,
			"create accel class device file failed!\n");
		goto exit_err_clean;
	}
	#endif
	/* set sensor PMU into suspend power mode for all */
	if (smi_pmu_set_suspend(client_data) < 0) {
		dev_err(dev, "Failed to set SMI130 to suspend power mode\n");
		goto exit_err_sysfs;
	}
	/*enable the data ready interrupt*/
	SMI_CALL_API(set_intr_enable_1)(SMI130_DATA_RDY_ENABLE, 1);
	dev_notice(dev, "sensor_time:%d, %d, %d",
		sensortime_duration_tbl[0].ts_delat,
		sensortime_duration_tbl[0].ts_duration_lsb,
		sensortime_duration_tbl[0].ts_duration_us);
	dev_notice(dev, "sensor %s probed successfully", SENSOR_NAME);

	return 0;

exit_err_sysfs:
	if (err)
		smi_input_destroy(client_data);

exit_err_clean:
	if (err) {
		if (client_data != NULL) {
			if (NULL != client_data->bosch_pd) {
				kfree(client_data->bosch_pd);
				client_data->bosch_pd = NULL;
			}
		}
	}
	return err;
}
EXPORT_SYMBOL(smi_probe);

/*!
 * @brief remove smi client
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
int smi_remove(struct device *dev)
{
	int err = 0;
	struct smi_client_data *client_data = dev_get_drvdata(dev);

	if (NULL != client_data) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&client_data->early_suspend_handler);
#endif
		mutex_lock(&client_data->mutex_enable);
		if (SMI_ACC_PM_NORMAL == client_data->pw.acc_pm ||
			SMI_GYRO_PM_NORMAL == client_data->pw.gyro_pm ||
				SMI_MAG_PM_NORMAL == client_data->pw.mag_pm) {
			cancel_delayed_work_sync(&client_data->work);
		}
		mutex_unlock(&client_data->mutex_enable);

		err = smi_pmu_set_suspend(client_data);

		smi_delay(5);

		sysfs_remove_group(&client_data->input->dev.kobj,
				&smi130_attribute_group);
		smi_input_destroy(client_data);

		if (NULL != client_data->bosch_pd) {
			kfree(client_data->bosch_pd);
			client_data->bosch_pd = NULL;
		}
		kfree(client_data);
	}

	return err;
}
EXPORT_SYMBOL(smi_remove);

static int smi_post_resume(struct smi_client_data *client_data)
{
	int err = 0;

	mutex_lock(&client_data->mutex_enable);

	if (atomic_read(&client_data->wkqueue_en) == 1) {
		smi130_set_acc_op_mode(client_data, SMI_ACC_PM_NORMAL);
		schedule_delayed_work(&client_data->work,
				msecs_to_jiffies(
					atomic_read(&client_data->delay)));
	}
	mutex_unlock(&client_data->mutex_enable);

	return err;
}


int smi_suspend(struct device *dev)
{
	int err = 0;
	struct smi_client_data *client_data = dev_get_drvdata(dev);
	unsigned char stc_enable;
	unsigned char std_enable;
	dev_err(client_data->dev, "smi suspend function entrance");

	atomic_set(&client_data->in_suspend, 1);
	if (atomic_read(&client_data->wkqueue_en) == 1) {
		smi130_set_acc_op_mode(client_data, SMI_ACC_PM_SUSPEND);
		cancel_delayed_work_sync(&client_data->work);
	}
	SMI_CALL_API(get_step_counter_enable)(&stc_enable);
	SMI_CALL_API(get_step_detector_enable)(&std_enable);
	if (client_data->pw.acc_pm != SMI_ACC_PM_SUSPEND &&
		(stc_enable != 1) && (std_enable != 1) &&
		(client_data->sig_flag != 1)) {
		err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_acc_arr[SMI_ACC_PM_SUSPEND]);
		smi_delay(3);
	}
	if (client_data->pw.gyro_pm != SMI_GYRO_PM_SUSPEND) {
		err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_SUSPEND]);
		smi_delay(3);
	}

	if (client_data->pw.mag_pm != SMI_MAG_PM_SUSPEND) {
#if defined(SMI130_AKM09912_SUPPORT)
		err += smi130_set_bosch_akm_and_secondary_if_powermode(
		SMI130_MAG_SUSPEND_MODE);
#else
		err += smi130_set_bmm150_mag_and_secondary_if_power_mode(
		SMI130_MAG_SUSPEND_MODE);
#endif
		smi_delay(3);
	}

	return err;
}
EXPORT_SYMBOL(smi_suspend);

int smi_resume(struct device *dev)
{
	int err = 0;
	struct smi_client_data *client_data = dev_get_drvdata(dev);
	atomic_set(&client_data->in_suspend, 0);
	if (client_data->pw.acc_pm != SMI_ACC_PM_SUSPEND) {
		err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_acc_arr[SMI_ACC_PM_NORMAL]);
		smi_delay(3);
	}
	if (client_data->pw.gyro_pm != SMI_GYRO_PM_SUSPEND) {
		err += SMI_CALL_API(set_command_register)
				(smi_pmu_cmd_gyro_arr[SMI_GYRO_PM_NORMAL]);
		smi_delay(3);
	}

	if (client_data->pw.mag_pm != SMI_MAG_PM_SUSPEND) {
#if defined(SMI130_AKM09912_SUPPORT)
		err += smi130_set_bosch_akm_and_secondary_if_powermode
					(SMI130_MAG_FORCE_MODE);
#else
		err += smi130_set_bmm150_mag_and_secondary_if_power_mode
					(SMI130_MAG_FORCE_MODE);
#endif
		smi_delay(3);
	}
	/* post resume operation */
	err += smi_post_resume(client_data);

	return err;
}
EXPORT_SYMBOL(smi_resume);

