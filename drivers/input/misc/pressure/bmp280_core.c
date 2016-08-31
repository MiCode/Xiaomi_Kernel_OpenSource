/*!
 * @section LICENSE
 * $license$
 *
 * @filename $filename$
 * @date	 $date$
 * @id		 $id$
 *
 * @brief
 * The core code of BMP280 device driver
 *
 * @detail
 * This file implements the core code of BMP280 device driver,
 * which includes hardware related functions, input device register,
 * device attribute files, etc.
 * This file calls some functions defined in BMP280.c and could be
 * called by bmp280_i2c.c and bmp280_spi.c separately.
 *
 * ////////////////////////////////////////////////////////////////
 *            Hardware Abstration Layer                | user space
 * ----------------------------------------------------------------
 *                                | |                  |
 *              _________________\| |/_____________    |
 *             |                  \ /              |   |
 *             |             --------------        |   |
 *bmp280_i2c.c-|----|        | sysfs nodes |       |   |
 *             |    |        --------------        |   |
 *             |   \|/                             |   |
 *             | |-----|                           |   |
 *             | |probe|      bmp280_core.c        |   |
 *             | |-----|                           |   |
 *             |   /|\                             |   |kernel space
 *             |    |                              |   |
 *bmp280_spi.c-|----|                              |   |
 *             |                                   |   |
 *             |___________________________________|   |
 *                                | |                  |
 *                               \| |/                 |
 *                           _____\ /_____             |
 *                           |            |            |
 *                           |  BMP280.c  |            |
 *                           |____________|            |
 * //////////////////////////////////////////////////////////////////
 *
 *  REVISION: V1.3
 *  HISTORY:  V1.0 --- Driver code creation
 *            V1.1 --- 1. Modify standby duration definition.
 *                     2. Change default workmode from ultra high resolution
 *                        to standard resolution.
 *                     3. Change default filter coefficient from x16 to x8.
 *            V1.2 --- Use EV_MSC instead of EV_ABS
 *            V1.3 --- 1. Support new chip id 0x57.
 *                     2. Add selftest sysfs node.
*/


#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "bmp280_core.h"

/*! @defgroup bmp280_core_src
 *  @brief The core code of BMP280 device driver
 @{*/
/*! define sensor chip id [BMP280 = 0x56 or 0x57],[... = ...] */
#define BMP_SENSOR_CHIP_ID_1	0x56
#define BMP_SENSOR_CHIP_ID_2	0x57
#define BMP_SENSOR_CHIP_ID_3	0x58
/*! define sensor i2c address */
#define BMP_I2C_ADDRESS		BMP280_I2C_ADDRESS
/*! define minimum pressure value by input event */
#define ABS_MIN_PRESSURE	30000
/*! define maximum pressure value by input event */
#define ABS_MAX_PRESSURE	110000
#define ABS_PRESSURE_FUZZ	5
#define ABS_PRESSURE_FLAT	5

/*! define default delay time used by input event [unit:ms] */
#define BMP_DELAY_DEFAULT	200
/*! define maximum temperature oversampling */
#define BMP_OVERSAMPLING_T_MAX	BMP_VAL_NAME(OVERSAMPLING_16X)
/*! define maximum pressure oversampling */
#define BMP_OVERSAMPLING_P_MAX	BMP_VAL_NAME(OVERSAMPLING_16X)
/*! define defalut filter coefficient */
#define BMP_FILTER_DEFAULT	BMP_VAL_NAME(FILTERCOEFF_8)
/*! define maximum filter coefficient */
#define BMP_FILTER_MAX		BMP_VAL_NAME(FILTERCOEFF_16)
/*! define default work mode */
#define BMP_WORKMODE_DEFAULT		BMP_VAL_NAME(STANDARDRESOLUTION_MODE)
/*! define default standby duration [unit:ms] */
#define BMP_STANDBYDUR_DEFAULT	1
/*! define i2c interface disable switch */
#define BMP280_I2C_DISABLE_SWITCH	0x87
/*! no action to selftest */
#define BMP_SELFTEST_NO_ACTION	-1
/*! selftest failed */
#define BMP_SELFTEST_FAILED	0
/*! selftest success */
#define BMP_SELFTEST_SUCCESS	1

/*!
 * @brief Each client has this additional data, this particular
 * data structure is defined for bmp280 client
*/
struct bmp_client_data {
	/*!data bus for hardware communication */
	struct bmp_data_bus data_bus;
	/*!device information used by sensor API */
	struct bmp280_t device;
	/*!device register to kernel device model */
	struct device *dev;
	/*!mutex lock variable */
	struct mutex lock;

	/*!temperature oversampling variable */
	u8 oversampling_t;
	/*!pressure oversampling variable */
	u8 oversampling_p;
	/*!indicate operation mode */
	u8 op_mode;
	/*!indicate filter coefficient */
	u8 filter;
	/*!indicate standby duration */
	u32 standbydur;
	/*!indicate work mode */
	u8 workmode;
#ifdef CONFIG_HAS_EARLYSUSPEND
	/*!early suspend variable */
	struct early_suspend early_suspend;
#endif
	/*!indicate input device; register to input core in kernel */
	struct input_dev *input;
	/*!register to work queue */
	struct delayed_work work;
	/*!delay time used by input event */
	u32 delay;
	/*!enable/disable sensor output */
	u32 enable;
	/*! indicate selftest status
	* -1: no action
	*  0: failed
	*  1: success
	*/
	s8 selftest;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bmp_early_suspend(struct early_suspend *h);
static void bmp_late_resume(struct early_suspend *h);
#endif

/*!
 * @brief list all the standby duration
 * that could be set[unit:ms]
*/
static const u32 standbydur_list[] = {
	1, 63, 125, 250, 500, 1000, 2000, 4000};
/*!
 * @brief list all the sensor operation modes
*/
static const u8 op_mode_list[] = {
	BMP_VAL_NAME(SLEEP_MODE),
	BMP_VAL_NAME(FORCED_MODE),
	BMP_VAL_NAME(NORMAL_MODE)
};
/*!
 * @brief list all the sensor work modes
*/
static const u8 workmode_list[] = {
	BMP_VAL_NAME(ULTRALOWPOWER_MODE),
	BMP_VAL_NAME(LOWPOWER_MODE),
	BMP_VAL_NAME(STANDARDRESOLUTION_MODE),
	BMP_VAL_NAME(HIGHRESOLUTION_MODE),
	BMP_VAL_NAME(ULTRAHIGHRESOLUTION_MODE)
};

/*!
 * @brief implement delay function
 *
 * @param msec	  millisecond numbers
 *
 * @return no return value
*/
static void bmp_delay(u16 msec)
{
	mdelay(msec);
}

/*!
 * @brief check bmp sensor's chip id
 *
 * @param data_bus the pointer of data bus
 *
 * @return zero success, non-zero failed
 * @retval zero  success
 * @retval -EIO communication error
 * @retval -ENODEV chip id dismatch
*/
static int bmp_check_chip_id(struct bmp_data_bus *data_bus)
{
	int err = 0;
	u8 chip_id = 0;

	err = data_bus->bops->bus_read(BMP_I2C_ADDRESS, \
		BMP_REG_NAME(CHIPID_REG), &chip_id, 1);
	if (err < 0) {
		err = -EIO;
		PERR("bus read failed\n");
		return err;
	}

	if (BMP_SENSOR_CHIP_ID_1 != chip_id &&
		BMP_SENSOR_CHIP_ID_2 != chip_id &&
		BMP_SENSOR_CHIP_ID_3 != chip_id) {
		err = -ENODEV;
		PERR("read %s chip id failed, read value = %d\n", \
				BMP_NAME, chip_id);
		return err;
	}
	PINFO("read %s chip id successfully", BMP_NAME);

	return err;
}

/*!
 * @brief get compersated temperature value
 *
 * @param data the pointer of bmp client data
 * @param temperature the pointer of temperature value
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_get_temperature(struct bmp_client_data *data, s32 *temperature)
{
	s32 utemperature;
	int err = 0;

	err = BMP_CALL_API(read_ut)(&utemperature);
	if (err)
		return err;

	*temperature = BMP_CALL_API(compensate_T_int32)(utemperature);
	return err;
}

/*!
 * @brief get compersated pressure value
 *
 * @param data the pointer of bmp client data
 * @param pressure the pointer of pressure value
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_get_pressure(struct bmp_client_data *data, u32 *pressure)
{
	s32 temperature;
	s32 upressure;
	int err = 0;

	/*
	 *get current temperature to compersate pressure value
	 *via variable t_fine, which is defined in sensor function API
	 */
	err = bmp_get_temperature(data, &temperature);
	if (err)
		return err;

	err = BMP_CALL_API(read_up)(&upressure);
	if (err)
		return err;

	*pressure = (BMP_CALL_API(compensate_P_int64)(upressure))>>8;
	return err;
}

/*!
 * @brief get temperature oversampling
 *
 * @param data the pointer of bmp client data
 *
 * @return temperature oversampling value
 * @retval 0 oversampling skipped
 * @retval 1 oversampling1X
 * @retval 2 oversampling2X
 * @retval 3 oversampling4X
 * @retval 4 oversampling8X
 * @retval 5 oversampling16X
*/
static u32 bmp_get_oversampling_t(struct bmp_client_data *data)
{
	int err = 0;

	err = BMP_CALL_API(get_osrs_t)(&data->oversampling_t);
	if (err)
		return err;

	return data->oversampling_t;
}

/*!
 * @brief set temperature oversampling
 *
 * @param data the pointer of bmp client data
 * @param oversampling temperature oversampling value need to set
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_set_oversampling_t(struct bmp_client_data *data, u8 oversampling)
{
	int err = 0;

	if (oversampling > BMP_OVERSAMPLING_T_MAX)
		oversampling = BMP_OVERSAMPLING_T_MAX;

	err = BMP_CALL_API(set_osrs_t)(oversampling);
	if (err)
		return	err;

	data->oversampling_t = oversampling;
	return err;
}

/*!
 * @brief get pressure oversampling
 *
 * @param data the pointer of bmp client data
 *
 * @return pressure oversampling value
 * @retval 0 oversampling skipped
 * @retval 1 oversampling1X
 * @retval 2 oversampling2X
 * @retval 3 oversampling4X
 * @retval 4 oversampling8X
 * @retval 5 oversampling16X
*/
static u32 bmp_get_oversampling_p(struct bmp_client_data *data)
{
	int err = 0;

	err = BMP_CALL_API(get_osrs_p)(&data->oversampling_p);
	if (err)
		return err;

	return data->oversampling_p;
}

/*!
 * @brief set pressure oversampling
 *
 * @param data the pointer of bmp client data
 * @param oversampling pressure oversampling value needed to set
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_set_oversampling_p(struct bmp_client_data *data, u8 oversampling)
{
	int err = 0;

	if (oversampling > BMP_OVERSAMPLING_P_MAX)
		oversampling = BMP_OVERSAMPLING_P_MAX;

	err = BMP_CALL_API(set_osrs_p)(oversampling);
	if (err)
		return err;

	data->oversampling_p = oversampling;
	return err;
}

/*!
 * @brief get operation mode
 *
 * @param data the pointer of bmp client data
 *
 * @return operation mode
 * @retval 0  SLEEP MODE
 * @retval 1  FORCED MODE
 * @retval 3  NORMAL MODE
*/
static u32 bmp_get_op_mode(struct bmp_client_data *data)
{
	int err = 0;

	err = BMP_CALL_API(get_mode)(&data->op_mode);
	if (err)
		return err;

	if (data->op_mode == 0x01 || data->op_mode == 0x02)
		data->op_mode = BMP_VAL_NAME(FORCED_MODE);

	return data->op_mode;
}

/*!
 * @brief set operation mode
 *
 * @param data the pointer of bmp client data
 * @param op_mode operation mode need to set
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_set_op_mode(struct bmp_client_data *data, u8 op_mode)
{
	int err = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(op_mode_list); i++) {
		if (op_mode_list[i] == op_mode)
			break;
	}

	if (ARRAY_SIZE(op_mode_list) <= i)
		return -EPERM;

	err = BMP_CALL_API(set_mode)(op_mode);
	if (err)
		return err;

	data->op_mode = op_mode;
	return err;
}

/*!
 * @brief get filter coefficient
 *
 * @param data the pointer of bmp client data
 *
 * @return filter coefficient value
 * @retval 0 filter off
 * @retval 1 filter 2
 * @retval 2 filter 4
 * @retval 3 filter 8
 * @retval 4 filter 16
*/
static u32 bmp_get_filter(struct bmp_client_data *data)
{
	int err = 0;

	err = BMP_CALL_API(get_filter)(&data->filter);
	if (err)
		return err;

	return data->filter;
}

/*!
 * @brief set filter coefficient
 *
 * @param data the pointer of bmp client data
 * @param filter filter coefficient need to set
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_set_filter(struct bmp_client_data *data, u8 filter)
{
	int err = 0;

	if (filter > BMP_FILTER_MAX)
		filter = BMP_FILTER_MAX;

	err = BMP_CALL_API(set_filter)(filter);
	if (err)
		return err;

	data->filter = filter;
	return err;
}

/*!
 * @brief get standby duration
 *
 * @param data the pointer of bmp client data
 *
 * @return standby duration value
 * @retval 1 0.5ms
 * @retval 63 62.5ms
 * @retval 125 125ms
 * @retval 250 250ms
 * @retval 500 500ms
 * @retval 1000 1000ms
 * @retval 2000 2000ms
 * @retval 4000 4000ms
*/
static u32 bmp_get_standbydur(struct bmp_client_data *data)
{
	int err = 0;
	u8 standbydur;

	err = BMP_CALL_API(get_standbydur)(&standbydur);
	if (err)
		return err;

	data->standbydur = standbydur_list[standbydur];
	return data->standbydur;
}

/*!
 * @brief set standby duration
 *
 * @param data the pointer of bmp client data
 * @param standbydur standby duration need to set
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_set_standbydur(struct bmp_client_data *data, u32 standbydur)
{
	int err = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(standbydur_list); i++) {
		if (standbydur_list[i] == standbydur)
			break;
	}

	if (ARRAY_SIZE(standbydur_list) <= i)
		return -EPERM;

	err = BMP_CALL_API(set_standbydur)(i);
	if (err)
		return err;

	data->standbydur = standbydur;
	return err;
}

/*!
 * @brief get work mode
 *
 * @param data the pointer of bmp client data
 *
 * @return work mode
 * @retval 0 ULTRLOWPOWER MODE
 * @retval 1 LOWPOWER MODE
 * @retval 2 STANDARDSOLUTION MODE
 * @retval 3 HIGHRESOLUTION MODE
 * @retval 4 ULTRAHIGHRESOLUTION MODE
*/
static unsigned char bmp_get_workmode(struct bmp_client_data *data)
{
	return data->workmode;
}

/*!
 * @brief set work mode, which is defined by software, not hardware.
 * This setting will impact oversampling value of sensor.
 *
 * @param data the pointer of bmp client data
 * @param workmode work mode need to set
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_set_workmode(struct bmp_client_data *data, u8 workmode)
{
	int err = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(workmode_list); i++) {
		if (workmode_list[i] == workmode)
			break;
	}

	if (ARRAY_SIZE(workmode_list) <= i)
		return -EPERM;

	err = BMP_CALL_API(set_workmode)(workmode);
	if (err)
		return err;
	else
		data->workmode = workmode;

	bmp_get_oversampling_t(data);
	bmp_get_oversampling_p(data);

	return err;
}

/*!
 * @brief verify i2c disable switch status
 *
 * @param data the pointer of bmp client data
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp280_verify_i2c_disable_switch (struct bmp_client_data *data)
{
	int err = 0;
	u8 reg_val = 0xFF;

	err = BMP_CALL_API(read_register)
		(BMP280_I2C_DISABLE_SWITCH, &reg_val, 1);
	if (err < 0) {
		err = -EIO;
		PERR("bus read failed\n");
		return err;
	}

	if (reg_val == 0x00) {
		PINFO("bmp280 i2c interface is available\n");
		return 0;
	}

	PERR("verification of i2c interface is failure\n");
	return -EPERM;
}

/*!
 * @brief verify calibration parameters
 *
 * @param data the pointer of bmp client data
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp280_verify_calib_param(struct bmp_client_data *data)
{
	struct bmp280_calibration_param_t *cali = &(data->device.cal_param);

	/* verify that not all calibration parameters are 0 */
	if (cali->dig_T1 == 0 && cali->dig_T2 == 0 && cali->dig_T3 == 0
		&& cali->dig_P1 == 0 &&	cali->dig_P2 == 0
		&& cali->dig_P3 == 0 && cali->dig_P4 == 0
		&& cali->dig_P5 == 0 && cali->dig_P6 == 0
		&& cali->dig_P7 == 0 && cali->dig_P8 == 0
		&& cali->dig_P9 == 0) {
		PERR("all calibration parameters are zero\n");
		return -EPERM;
	}

	/* verify whether all the calibration parameters are within range */
	if (cali->dig_T1 < 19000 || cali->dig_T1 > 35000)
		return -EPERM;
	else if (cali->dig_T2 < 22000 || cali->dig_T2 > 30000)
		return -EPERM;
	else if (cali->dig_T3 < -3000 || cali->dig_T3 > -1000)
		return -EPERM;
	else if (cali->dig_P1 < 30000 || cali->dig_P1 > 42000)
		return -EPERM;
	else if (cali->dig_P2 < -12970 || cali->dig_P2 > -8000)
		return -EPERM;
	else if (cali->dig_P3 < -5000 || cali->dig_P3 > 8000)
		return -EPERM;
	else if (cali->dig_P4 < -10000 || cali->dig_P4 > 18000)
		return -EPERM;
	else if (cali->dig_P5 < -500 || cali->dig_P5 > 1100)
		return -EPERM;
	else if (cali->dig_P6 < -1000 || cali->dig_P6 > 1000)
		return -EPERM;
	else if (cali->dig_P7 < -32768 || cali->dig_P7 > 32767)
		return -EPERM;
	else if (cali->dig_P8 < -30000 || cali->dig_P8 > 10000)
		return -EPERM;
	else if (cali->dig_P9 < -10000 || cali->dig_P9 > 30000)
		return -EPERM;

	PINFO("calibration parameters are OK\n");
	return 0;
}

/*!
 * @brief verify compensated temperature and pressure value
 *
 * @param data the pointer of bmp client data
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp280_verify_pt(struct bmp_client_data *data)
{
	u8 wait_time = 0;
	s32 temperature;
	u32 pressure;

	bmp_set_workmode(data, BMP_VAL_NAME(ULTRALOWPOWER_MODE));
	bmp_set_op_mode(data, BMP_VAL_NAME(FORCED_MODE));
	BMP_CALL_API(compute_wait_time)(&wait_time);
	bmp_delay(wait_time);
	bmp_get_temperature(data, &temperature);
	if (temperature <= 0 || temperature >= 40*100) {
		PERR("temperature value is out of range:%d*0.01degree\n",
			temperature);
		return -EPERM;
	}
	bmp_get_pressure(data, &pressure);
	if (pressure <= 900*100 || pressure >= 1100*100) {
		PERR("pressure value is out of range:%d Pa\n", pressure);
		return -EPERM;
	}

	PINFO("bmp280 temperature and pressure values are OK\n");
	return 0;
}

/*!
 * @brief do selftest
 *
 * @param data the pointer of bmp client data
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_do_selftest(struct bmp_client_data *data)
{
	int err = 0;

	if (data == NULL)
		return -EINVAL;

	err = bmp280_verify_i2c_disable_switch (data);
	if (err) {
		data->selftest = 0;
		return BMP_SELFTEST_FAILED;
	}

	err = bmp280_verify_calib_param(data);
	if (err) {
		data->selftest = 0;
		return BMP_SELFTEST_FAILED;
	}

	err = bmp280_verify_pt(data);
	if (err) {
		data->selftest = 0;
		return BMP_SELFTEST_FAILED;
	}

	/* selftest is OK */
	data->selftest = 1;
	return BMP_SELFTEST_SUCCESS;
}

/* sysfs callbacks */
/*!
 * @brief get delay value via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of delay buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_delay(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", data->delay);
}

/*!
 * @brief set delay value via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of delay buffer
 * @param count buffer size
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t store_delay(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	unsigned long delay;
	int status = kstrtoul(buf, 10, &delay);
	if (status == 0) {
		mutex_lock(&data->lock);
		data->delay = delay;
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

/*!
 * @brief get compersated temperature value via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of temperature buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_temperature(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	s32 temperature;
	int status;
	struct bmp_client_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->lock);
	status = bmp_get_temperature(data, &temperature);
	mutex_unlock(&data->lock);
	if (status == 0)
		return sprintf(buf, "%d\n", temperature);

	return status;
}

/*!
 * @brief set compersated pressure value via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of pressure buffer
 * @param count buffer size
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_pressure(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u32 pressure;
	int status;
	struct bmp_client_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->lock);
	status = bmp_get_pressure(data, &pressure);
	mutex_unlock(&data->lock);
	if (status == 0)
		return sprintf(buf, "%d\n", pressure);

	return status;
}

/*!
 * @brief get temperature oversampling value via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of temperature oversampling buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_oversampling_t(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", bmp_get_oversampling_t(data));
}

/*!
 * @brief set temperature oversampling value via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of temperature oversampling buffer
 * @param count buffer size
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t store_oversampling_t(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	unsigned long oversampling;
	int status = kstrtoul(buf, 10, &oversampling);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_oversampling_t(data, oversampling);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

/*!
 * @brief get pressure oversampling value via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of pressure oversampling buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_oversampling_p(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", bmp_get_oversampling_p(data));
}

/*!
 * @brief set pressure oversampling value via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of pressure oversampling buffer
 * @param count buffer size
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t store_oversampling_p(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	unsigned long oversampling;
	int status = kstrtoul(buf, 10, &oversampling);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_oversampling_p(data, oversampling);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

/*!
 * @brief get operation mode via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of operation mode buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_op_mode(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", bmp_get_op_mode(data));
}

/*!
 * @brief set operation mode via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of operation mode buffer
 * @param count buffer size
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t store_op_mode(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	unsigned long op_mode;
	int status = kstrtoul(buf, 10, &op_mode);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_op_mode(data, op_mode);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

/*!
 * @brief get filter coefficient via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of filter coefficient buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_filter(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", bmp_get_filter(data));
}

/*!
 * @brief set filter coefficient via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of filter coefficient buffer
 * @param count buffer size
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t store_filter(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	unsigned long filter;
	int status = kstrtoul(buf, 10, &filter);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_filter(data, filter);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

/*!
 * @brief get standby duration via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of standby duration buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_standbydur(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", bmp_get_standbydur(data));
}

/*!
 * @brief set standby duration via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of standby duration buffer
 * @param count buffer size
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t store_standbydur(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	unsigned long standbydur;
	int status = kstrtoul(buf, 10, &standbydur);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_standbydur(data, standbydur);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

/*!
 * @brief get work mode via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of work mode buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_workmode(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", bmp_get_workmode(data));
}

/*!
 * @brief set work mode via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of work mode buffer
 * @param count buffer size
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t store_workmode(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	unsigned long workmode;
	int status = kstrtoul(buf, 10, &workmode);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_workmode(data, workmode);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

/*!
 * @brief get sensor work state via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of enable/disable value buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", data->enable);
}

/*!
 * @brief enable/disable sensor function via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of enable/disable buffer
 * @param count buffer size
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t store_enable(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	unsigned long enable;
	int status = kstrtoul(buf, 10, &enable);
	if (status == 0) {
		enable = enable ? 1 : 0;
		mutex_lock(&data->lock);
		if (data->enable != enable) {
			if (enable) {
				#ifdef CONFIG_PM
				bmp_enable(dev);
				#endif
				bmp_set_op_mode(data, \
					BMP_VAL_NAME(NORMAL_MODE));
				schedule_delayed_work(&data->work,
					msecs_to_jiffies(data->delay));
			} else {
				cancel_delayed_work_sync(&data->work);
				bmp_set_op_mode(data, \
					BMP_VAL_NAME(SLEEP_MODE));
				#ifdef CONFIG_PM
				bmp_disable(dev);
				#endif
			}
			data->enable = enable;
		}
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

/*!
 * @brief get selftest status via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of selftest buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_selftest(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", data->selftest);
}

/*!
 * @brief do selftest via sysfs node
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of selftest buffer
 * @param count buffer size
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t store_selftest(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
	unsigned long action;
	int status = kstrtoul(buf, 10, &action);
	if (0 != status)
		return status;

	/* 1 means do selftest */
	if (1 != action)
		return -EINVAL;

	mutex_lock(&data->lock);
	status = bmp_do_selftest(data);
	mutex_unlock(&data->lock);

	if (BMP_SELFTEST_SUCCESS == status)
		return count;
	else
		return status;
}

#ifdef DEBUG_BMP280
/*!
 * @brief dump significant registers value from hardware
 * and copy to use space via sysfs node. This node only for debug,
 * which could dump registers from 0xF3 to 0xFC.
 *
 * @param dev the pointer of device
 * @param attr the pointer of device attribute file
 * @param buf the pointer of registers value buffer
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static ssize_t show_dump_reg(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	#define REG_CALI_NUM (0xA1 - 0x88 + 1)
	#define REG_CTRL_NUM (0xFC - 0xF3 + 1)
	struct bmp_client_data *data = dev_get_drvdata(dev);
	char regsbuf[REG_CALI_NUM + REG_CTRL_NUM];
	char strbuf[REG_CALI_NUM + REG_CTRL_NUM + 256] = {0};
	int err = 0, i;

	sprintf(strbuf + strlen(strbuf), \
			"-----calib regs[0x88 ~ 0xA1]-----\n");
	err = data->data_bus.bops->bus_read(BMP_I2C_ADDRESS, \
			0x88, regsbuf, REG_CALI_NUM);
	if (err)
		return err;

	for (i = 0; i < REG_CALI_NUM; i++)
		sprintf(strbuf + strlen(strbuf), "%02X%c", \
				regsbuf[i], ((i+1)%0x10 == 0) ? '\n' : ' ');

	sprintf(strbuf + strlen(strbuf), \
			"\n-----ctrl regs[0xF3 ~ 0xFC]-----\n");
	err = data->data_bus.bops->bus_read(BMP_I2C_ADDRESS, \
		0xF3, regsbuf, REG_CTRL_NUM);
	if (err)
		return err;

	for (i = 0; i < REG_CTRL_NUM; i++)
		sprintf(strbuf + strlen(strbuf), "%02X ", regsbuf[i]);

	return sprintf(buf, 4096, "%s\n", strbuf);
}
#endif/*DEBUG_BMP280*/

static DEVICE_ATTR(delay, S_IWUSR | S_IRUGO,
			show_delay, store_delay);
static DEVICE_ATTR(temperature, S_IRUGO,
			show_temperature, NULL);
static DEVICE_ATTR(pressure, S_IRUGO,
			show_pressure, NULL);
static DEVICE_ATTR(oversampling_t, S_IWUSR | S_IRUGO,
			show_oversampling_t, store_oversampling_t);
static DEVICE_ATTR(oversampling_p, S_IWUSR | S_IRUGO,
			show_oversampling_p, store_oversampling_p);
static DEVICE_ATTR(op_mode, S_IWUSR | S_IRUGO,
			show_op_mode, store_op_mode);
static DEVICE_ATTR(filter, S_IWUSR | S_IRUGO,
			show_filter, store_filter);
static DEVICE_ATTR(standbydur, S_IWUSR | S_IRUGO,
			show_standbydur, store_standbydur);
static DEVICE_ATTR(workmode, S_IWUSR | S_IRUGO,
			show_workmode, store_workmode);
static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
			show_enable, store_enable);
static DEVICE_ATTR(selftest, S_IWUSR | S_IRUGO,
			show_selftest, store_selftest);
#ifdef DEBUG_BMP280
static DEVICE_ATTR(dump_reg, S_IRUGO,
			show_dump_reg, NULL);
#endif

/*!
 * @brief device attribute files
*/
static struct attribute *bmp_attributes[] = {
	/**< delay attribute */
	&dev_attr_delay.attr,
	/**< compersated temperature attribute */
	&dev_attr_temperature.attr,
	/**< compersated pressure attribute */
	&dev_attr_pressure.attr,
	/**< temperature oversampling attribute */
	&dev_attr_oversampling_t.attr,
	/**< pressure oversampling attribute */
	&dev_attr_oversampling_p.attr,
	/**< operature mode attribute */
	&dev_attr_op_mode.attr,
	/**< filter coefficient attribute */
	&dev_attr_filter.attr,
	/**< standby duration attribute */
	&dev_attr_standbydur.attr,
	/**< work mode attribute */
	&dev_attr_workmode.attr,
	/**< enable/disable attribute */
	&dev_attr_enable.attr,
	/**< selftest attribute */
	&dev_attr_selftest.attr,
	/**< dump registers attribute */
#ifdef DEBUG_BMP280
	&dev_attr_dump_reg.attr,
#endif
	/**< flag to indicate the end */
	NULL
};

/*!
 * @brief attribute files group
*/
static const struct attribute_group bmp_attr_group = {
	/**< bmp attributes */
	.attrs = bmp_attributes,
};

/*!
 * @brief workqueue function to report input event
 *
 * @param work the pointer of workqueue
 *
 * @return no return value
*/
static void bmp_work_func(struct work_struct *work)
{
	struct bmp_client_data *client_data =
		container_of((struct delayed_work *)work,
		struct bmp_client_data, work);
	u32 delay = msecs_to_jiffies(client_data->delay);
	u32 j1 = jiffies;
	u32 pressure;
	int status;

	mutex_lock(&client_data->lock);
	status = bmp_get_pressure(client_data, &pressure);
	mutex_unlock(&client_data->lock);
	if (status == 0) {
		input_event(client_data->input, EV_ABS, ABS_PRESSURE, pressure);
		input_sync(client_data->input);
	}

	schedule_delayed_work(&client_data->work, delay-(jiffies-j1));
}

/*!
 * @brief initialize input device
 *
 * @param data the pointer of bmp client data
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_input_init(struct bmp_client_data *data)
{
	struct input_dev *dev;
	int err;

	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;
	dev->name = BMP_INPUT_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_PRESSURE);
	input_set_drvdata(dev, data);
	input_set_abs_params(dev, ABS_PRESSURE,
			     ABS_MIN_PRESSURE, ABS_MAX_PRESSURE,
			     ABS_PRESSURE_FUZZ, ABS_PRESSURE_FLAT);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	data->input = dev;

	data->input->phys =
		kobject_get_path(&data->input->dev.kobj, GFP_KERNEL);
	if (data->input->phys == NULL) {
		dev_err(&data->dev, "fail to get sysfs path.");
		input_unregister_device(data->input);
		return -ENOMEM;
	}

	return 0;
}

/*!
 * @brief delete input device
 *
 * @param data the pointer of bmp client data
 *
 * @return no return value
*/
static void bmp_input_delete(struct bmp_client_data *data)
{
	struct input_dev *dev = data->input;

	if (dev->phys)
		kfree(dev->phys);
	input_unregister_device(dev);
	input_free_device(dev);
}

/*!
 * @brief initialize bmp client
 *
 * @param data the pointer of bmp client data
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
static int bmp_init_client(struct bmp_client_data *data)
{
	int status;

	data->device.bus_read = data->data_bus.bops->bus_read;
	data->device.bus_write = data->data_bus.bops->bus_write;
	data->device.delay_msec = bmp_delay;

	status = BMP_CALL_API(init)(&data->device);
	if (status)
		return status;

	mutex_init(&data->lock);

	data->delay  = BMP_DELAY_DEFAULT;
	data->enable = 0;
	data->selftest = BMP_SELFTEST_NO_ACTION;/* no action to selftest */

	status = bmp_set_op_mode(data, BMP_VAL_NAME(SLEEP_MODE));
	if (status)
		return status;

	status = bmp_set_filter(data, BMP_FILTER_DEFAULT);
	if (status)
		return status;

	status = bmp_set_standbydur(data, BMP_STANDBYDUR_DEFAULT);
	if (status)
		return status;

	status = bmp_set_workmode(data, BMP_WORKMODE_DEFAULT);
	return status;
}

/*!
 * @brief probe bmp sensor
 *
 * @param dev the pointer of device
 * @param data_bus the pointer of data bus communication
 *
 * @return zero success, non-zero failed
 * @retval zero success
 * @retval non-zero failed
*/
int bmp_probe(struct device *dev, struct bmp_data_bus *data_bus)
{
	struct bmp_client_data *data;
	int err = 0;


	if (!dev || !data_bus) {
		err = -EINVAL;
		goto exit;
	}

	/* check chip id */
	err = bmp_check_chip_id(data_bus);
	if (err) {
		PERR("Bosch Sensortec Device not found, chip id mismatch!\n");
		goto exit;
	} else {
		PNOTICE("Bosch Sensortec Device %s detected.\n", BMP_NAME);
	}

	data = kzalloc(sizeof(struct bmp_client_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	dev_set_drvdata(dev, data);
	data->data_bus = *data_bus;
	data->dev = dev;

	/* Initialize the BMP chip */
	err = bmp_init_client(data);
	if (err != 0)
		goto exit_free;

	/* Initialize the BMP input device */
	err = bmp_input_init(data);
	if (err != 0)
		goto exit_free;

	/* Register sysfs hooks */
	err = sysfs_create_group(&data->input->dev.kobj, &bmp_attr_group);
	if (err)
		goto error_sysfs;

	/* workqueue init */
	INIT_DELAYED_WORK(&data->work, bmp_work_func);

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = bmp_early_suspend;
	data->early_suspend.resume = bmp_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	PINFO("Succesfully probe sensor %s\n", BMP_NAME);
	return 0;

error_sysfs:
	bmp_input_delete(data);
exit_free:
	kfree(data);
exit:
	return err;
}
EXPORT_SYMBOL(bmp_probe);

/*!
 * @brief remove bmp client
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
int bmp_remove(struct device *dev)
{
	struct bmp_client_data *data = dev_get_drvdata(dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &bmp_attr_group);
	kfree(data);

	return 0;
}
EXPORT_SYMBOL(bmp_remove);

#ifdef CONFIG_PM
/*!
 * @brief disable power
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
int bmp_disable(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(bmp_disable);

/*!
 * @brief enable power
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
int bmp_enable(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(bmp_enable);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
/*!
 * @brief early suspend function of bmp sensor
 *
 * @param h the pointer of early suspend
 *
 * @return no return value
*/
static void bmp_early_suspend(struct early_suspend *h)
{
	struct bmp_client_data *data =
		container_of(h, struct bmp_client_data, early_suspend);

	mutex_lock(&data->lock);
	if (data->enable) {
		cancel_delayed_work_sync(&data->work);
		bmp_set_op_mode(data, BMP_VAL_NAME(SLEEP_MODE));
		#ifdef CONFIG_PM
		(void) bmp_disable(data->dev);
		#endif
	}
	mutex_unlock(&data->lock);
}

/*!
 * @brief late resume function of bmp sensor
 *
 * @param h the pointer of early suspend
 *
 * @return no return value
*/
static void bmp_late_resume(struct early_suspend *h)
{
	struct bmp_client_data *data =
		container_of(h, struct bmp_client_data, early_suspend);

	mutex_lock(&data->lock);
	if (data->enable) {
		#ifdef CONFIG_PM
		(void) bmp_enable(data->dev);
		#endif
		bmp_set_op_mode(data, BMP_VAL_NAME(NORMAL_MODE));
		schedule_delayed_work(&data->work,
			msecs_to_jiffies(data->delay));
	}
	mutex_unlock(&data->lock);
}
#endif

MODULE_DESCRIPTION("BMP280 DRIVER CORE");
MODULE_LICENSE("GPLv2");
/*@}*/
