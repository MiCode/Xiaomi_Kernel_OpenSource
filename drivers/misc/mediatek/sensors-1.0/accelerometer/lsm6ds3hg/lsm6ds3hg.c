/* ST LSM6DS3H Accelerometer sensor driver
 *
 *
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

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>

#include <cust_acc.h>
#include <hwmsensor.h>
#include <accel.h>
#include "lsm6ds3hg.h"
#include "lsm6ds3hg_API.h"


/*---------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
#define CONFIG_LSM6DS3H_LOWPASS	/*apply low pass filter on output */
#define C_MAX_FIR_LENGTH (32)

/*----------------------------------------------------------------------------*/
#define LSM6DS3H_AXIS_X          0
#define LSM6DS3H_AXIS_Y          1
#define LSM6DS3H_AXIS_Z          2
#define LSM6DS3H_AXES_NUM        3
#define LSM6DS3H_DATA_LEN       6
#define LSM6DS3H_DEV_NAME        "LSM6DS3H_ACCEL"
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id lsm6ds3h_i2c_id[] = {
	{LSM6DS3H_DEV_NAME, 0}, {}
};

static int lsm6ds3h_init_flag = -1;


/*----------------------------------------------------------------------------*/
enum enum_ADX_TRC {
	ADX_TRC_FILTER = 0x01,
	ADX_TRC_RAWDATA = 0x02,
	ADX_TRC_IOCTL = 0x04,
	ADX_TRC_CALI = 0X08,
	ADX_TRC_INFO = 0X10,
};
#define ADX_TRC enum enum_ADX_TRC
/*----------------------------------------------------------------------------*/
enum ACCEL_TRC {
	ACCEL_TRC_FILTER = 0x01,
	ACCEL_TRC_RAWDATA = 0x02,
	ACCEL_TRC_IOCTL = 0x04,
	ACCEL_TRC_CALI = 0X08,
	ACCEL_TRC_INFO = 0X10,
	ACCEL_TRC_DATA = 0X20,
};
#define ACCEL_TRC enum enum_ACCEL_TRC
/*----------------------------------------------------------------------------*/
struct scale_factor {
	u8 whole;
	u8 fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution {
	struct scale_factor scalefactor;
	int sensitivity;
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
struct data_filter {
	s16 raw[C_MAX_FIR_LENGTH][LSM6DS3H_AXES_NUM];
	int sum[LSM6DS3H_AXES_NUM];
	int num;
	int idx;
};

struct acc_hw acc_cust;
static struct acc_hw *hw = &acc_cust;
/* For  driver get cust info */
struct acc_hw *get_cust_acc(void)
{
	return &acc_cust;
}

/*----------------------------------------------------------------------------*/
struct lsm6ds3h_i2c_data {
	struct i2c_client *client;
	struct acc_hw *hw;
	struct hwmsen_convert cvt;
	atomic_t layout;
	/*misc */
	struct work_struct eint_work;
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest;
	atomic_t filter;
	s16 cali_sw[LSM6DS3H_AXES_NUM + 1];

	/*data */
	s8 offset[LSM6DS3H_AXES_NUM + 1];	/*+1: for 4-byte alignment */
	s16 data[LSM6DS3H_AXES_NUM + 1];
	int sensitivity;
	u8 sample_rate;

#if defined(CONFIG_LSM6DS3H_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif
};
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int lsm6ds3h_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int lsm6ds3h_i2c_remove(struct i2c_client *client);
static int LSM6DS3H_init_client(struct i2c_client *client, bool enable);
static int LSM6DS3H_SetPowerMode(struct i2c_client *client, bool enable);

static int LSM6DS3H_ReadAccRawData(struct i2c_client *client,
	s16 data[LSM6DS3H_AXES_NUM]);
static int lsm6ds3h_suspend(struct device *dev);
static int lsm6ds3h_resume(struct device *dev);
static int LSM6DS3H_SetSampleRate(struct i2c_client *client, u8 sample_rate);

#if 0
static int LSM6DS3H_Enable_Func(struct i2c_client *client,
	LSM6DS3H_ACC_GYRO_FUNC_EN_t newValue);
static int LSM6DS3H_Int_Ctrl(struct i2c_client *client,
	LSM6DS3H_ACC_GYRO_INT_ACTIVE_t int_act,
	LSM6DS3H_ACC_GYRO_INT_LATCH_CTL_t int_latch);
#endif

static int lsm6ds3h_local_init(void);
static int lsm6ds3h_local_uninit(void);


static DEFINE_MUTEX(lsm6ds3h_init_mutex);
static DEFINE_MUTEX(lsm6ds3h_factory_mutex);


#ifdef CONFIG_OF
static const struct of_device_id accel_of_match[] = {
	{.compatible = "mediatek,gsensor"},
	{},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops lsm6ds3h_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lsm6ds3h_suspend, lsm6ds3h_resume)
};
#endif

static struct i2c_driver lsm6ds3h_i2c_driver = {
	.probe = lsm6ds3h_i2c_probe,
	.remove = lsm6ds3h_i2c_remove,

	.id_table = lsm6ds3h_i2c_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = LSM6DS3H_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
		.pm = &lsm6ds3h_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = accel_of_match, /*need add in dtsi first*/
#endif
		   },
};

static struct acc_init_info lsm6ds3h_init_info = {
	.name = LSM6DS3H_DEV_NAME,
	.init = lsm6ds3h_local_init,
	.uninit = lsm6ds3h_local_uninit,
};

/*----------------------------------------------------------------------------*/
struct i2c_client *lsm6ds3h_acc_i2c_client;

static struct lsm6ds3h_i2c_data *obj_i2c_data;
static bool sensor_power;
static bool enable_status;


/*----------------------------------------------------------------------------*/

#define GSE_TAG                  "[accel] "

#define GSE_FUN(f)               pr_debug(GSE_TAG"%s\n", __func__)

static int mpu_i2c_read_block(struct i2c_client *client, u8 addr,
	u8 *data, u8 len)
{
	int err = 0;
	u8 beg = addr;
	struct i2c_msg msgs[2] = { {0}, {0} };

	mutex_lock(&lsm6ds3h_init_mutex);
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	if (!client) {
		mutex_unlock(&lsm6ds3h_init_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		mutex_unlock(&lsm6ds3h_init_mutex);
		pr_info("[accel] length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != 2) {
		pr_info("[accel] i2c_transfer error: (%d %p %d) %d\n",
			addr, data, len, err);
		err = -EIO;
	} else
		err = 0;
	mutex_unlock(&lsm6ds3h_init_mutex);
	return err;

}

static int mpu_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data,
	u8 len)
{
/*because address also occupies one byte,
 * the maximum length for write is 7 bytes
 */
	int err = 0;
	int idx = 0;
	int num = 0;
	char buf[C_I2C_FIFO_SIZE];

	mutex_lock(&lsm6ds3h_init_mutex);
	if (!client) {
		mutex_unlock(&lsm6ds3h_init_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		mutex_unlock(&lsm6ds3h_init_mutex);
		pr_info("[accel]  length %d exceeds %d\n",
			len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		mutex_unlock(&lsm6ds3h_init_mutex);
		pr_info("[accel] i2c send command error!!\n");
		return -EFAULT;
	}
	mutex_unlock(&lsm6ds3h_init_mutex);
	return err;
}

int LSM6DS3H_hwmsen_read_block(u8 addr, u8 *buf, u8 len)
{
	if (lsm6ds3h_acc_i2c_client == NULL) {
		pr_info("[accel] %s null ptr!!\n", __func__);
		return LSM6DS3H_ERR_I2C;
	}
	return mpu_i2c_read_block(lsm6ds3h_acc_i2c_client, addr, buf, len);
}
EXPORT_SYMBOL(LSM6DS3H_hwmsen_read_block);

int LSM6DS3H_hwmsen_write_block(u8 addr, u8 *buf, u8 len)
{
	if (lsm6ds3h_acc_i2c_client == NULL) {
		pr_info("[accel] %s null ptr!!\n", __func__);
		return LSM6DS3H_ERR_I2C;
	}
	return mpu_i2c_write_block(lsm6ds3h_acc_i2c_client, addr, buf, len);
}
EXPORT_SYMBOL(LSM6DS3H_hwmsen_write_block);

static int LSM6DS3H_set_bank(struct i2c_client *client, u8 bank)
{
	int res = 0;
	u8 databuf[2];

	databuf[0] = bank;
	res = mpu_i2c_write_block(client, LSM6DS3H_WHO_AM_I, databuf, 0x1);
	if (res < 0) {
		pr_info("[accel] %s fail at %x", __func__, bank);
		return LSM6DS3H_ERR_I2C;
	}

	return LSM6DS3H_SUCCESS;
}

/*----------------------------------------------------------------------------*/

#if 1
static void LSM6DS3H_dumpReg(struct i2c_client *client)
{
	int i = 0;
	u8 addr = 0x10;
	u8 regdata = 0;

	for (i = 0; i < 25; i++) {
		/*dump all*/
		mpu_i2c_read_block(client, addr, &regdata, 0x01);
		pr_debug("[accel] Reg addr=%x regdata=%x\n", addr, regdata);
		addr++;
	}
}
#endif
static void LSM6DS3H_power(struct acc_hw *hw, unsigned int on)
{
	static unsigned int power_on;

	pr_debug("[accel] power %s\n", on ? "on" : "off");
#if 0
	if (power_on == on)	/*power status not change*/
		pr_debug("[accel] ignore power control: %d\n", on);
	else if (on) {		/* power on*/
		if (!hwPowerOn(hw->power_id, hw->power_vol, "LSM6DS3H"))
			pr_info("[accel] power on fails!!\n");
	} else {			/* power off*/
		if (!hwPowerDown(hw->power_id, "LSM6DS3H"))
			pr_info("[accel] power off fail!!\n");
	}
#endif
	power_on = on;
}

/*----------------------------------------------------------------------------*/

static int LSM6DS3H_write_rel_calibration(struct lsm6ds3h_i2c_data *obj,
	int dat[LSM6DS3H_AXES_NUM])
{
	obj->cali_sw[LSM6DS3H_AXIS_X] =
	    obj->cvt.sign[LSM6DS3H_AXIS_X] * dat[obj->cvt.map[LSM6DS3H_AXIS_X]];
	obj->cali_sw[LSM6DS3H_AXIS_Y] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Y] * dat[obj->cvt.map[LSM6DS3H_AXIS_Y]];
	obj->cali_sw[LSM6DS3H_AXIS_Z] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Z] * dat[obj->cvt.map[LSM6DS3H_AXIS_Z]];
#if DEBUG
	if (atomic_read(&obj->trace) & ACCEL_TRC_CALI) {
		pr_debug("[accel] test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n",
			obj->cvt.sign[LSM6DS3H_AXIS_X],
			obj->cvt.sign[LSM6DS3H_AXIS_Y],
			obj->cvt.sign[LSM6DS3H_AXIS_Z],
			dat[LSM6DS3H_AXIS_X],
			dat[LSM6DS3H_AXIS_Y],
			dat[LSM6DS3H_AXIS_Z],
			obj->cvt.map[LSM6DS3H_AXIS_X],
			obj->cvt.map[LSM6DS3H_AXIS_Y],
			obj->cvt.map[LSM6DS3H_AXIS_Z]);
		pr_debug("[accel] write calibration data  (%5d, %5d, %5d)\n",
			obj->cali_sw[LSM6DS3H_AXIS_X],
			obj->cali_sw[LSM6DS3H_AXIS_Y],
			obj->cali_sw[LSM6DS3H_AXIS_Z]);
	}
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3H_ResetCalibration(struct i2c_client *client)
{
	struct lsm6ds3h_i2c_data *obj = i2c_get_clientdata(client);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3H_ReadCalibration(struct i2c_client *client,
	int dat[LSM6DS3H_AXES_NUM])
{
	struct lsm6ds3h_i2c_data *obj = i2c_get_clientdata(client);

	dat[obj->cvt.map[LSM6DS3H_AXIS_X]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_X] * obj->cali_sw[LSM6DS3H_AXIS_X];
	dat[obj->cvt.map[LSM6DS3H_AXIS_Y]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Y] * obj->cali_sw[LSM6DS3H_AXIS_Y];
	dat[obj->cvt.map[LSM6DS3H_AXIS_Z]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Z] * obj->cali_sw[LSM6DS3H_AXIS_Z];

#if DEBUG
	if (atomic_read(&obj->trace) & ACCEL_TRC_CALI) {
		pr_debug("[accel] Read calibration data  (%5d, %5d, %5d)\n",
			dat[LSM6DS3H_AXIS_X], dat[LSM6DS3H_AXIS_Y],
			dat[LSM6DS3H_AXIS_Z]);
	}
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/

static int LSM6DS3H_WriteCalibration(struct i2c_client *client,
	int dat[LSM6DS3H_AXES_NUM])
{
	struct lsm6ds3h_i2c_data *obj = i2c_get_clientdata(client);
	int cali[LSM6DS3H_AXES_NUM];

	GSE_FUN();
	if (!obj || !dat) {
		pr_info("[accel] null ptr!!\n");
		return -EINVAL;
	}
	cali[obj->cvt.map[LSM6DS3H_AXIS_X]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_X] * obj->cali_sw[LSM6DS3H_AXIS_X];
	cali[obj->cvt.map[LSM6DS3H_AXIS_Y]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Y] * obj->cali_sw[LSM6DS3H_AXIS_Y];
	cali[obj->cvt.map[LSM6DS3H_AXIS_Z]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Z] * obj->cali_sw[LSM6DS3H_AXIS_Z];
	cali[LSM6DS3H_AXIS_X] += dat[LSM6DS3H_AXIS_X];
	cali[LSM6DS3H_AXIS_Y] += dat[LSM6DS3H_AXIS_Y];
	cali[LSM6DS3H_AXIS_Z] += dat[LSM6DS3H_AXIS_Z];
#if DEBUG
	if (atomic_read(&obj->trace) & ACCEL_TRC_CALI) {
		pr_debug("[accel] write accel calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
			dat[LSM6DS3H_AXIS_X], dat[LSM6DS3H_AXIS_Y],
			dat[LSM6DS3H_AXIS_Z],
			cali[LSM6DS3H_AXIS_X], cali[LSM6DS3H_AXIS_Y],
			cali[LSM6DS3H_AXIS_Z]);
	}
#endif
	return LSM6DS3H_write_rel_calibration(obj, cali);

}

/*----------------------------------------------------------------------------*/
static int LSM6DS3H_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);
	LSM6DS3H_set_bank(client, LSM6DS3H_FUNC_CFG_ACCESS);

	res = mpu_i2c_read_block(client, LSM6DS3H_WHO_AM_I, databuf, 0x1);
	pr_debug("[accel]  LSM6DS3H  id %x!\n", databuf[0]);
	if (databuf[0] != LSM6DS3H_FIXED_DEVID)
		return LSM6DS3H_ERR_IDENTIFICATION;

	if (res < 0)
		return LSM6DS3H_ERR_I2C;

	return LSM6DS3H_SUCCESS;
}


static int LSM6DS3H_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = { 0 };
	int res = 0;
	/*obj_i2c_data;*/
	struct lsm6ds3h_i2c_data *obj = i2c_get_clientdata(client);

	if (enable == sensor_power) {
		pr_debug("[accel] Sensor power status is newest!\n");
		return LSM6DS3H_SUCCESS;
	}

	if (mpu_i2c_read_block(client, LSM6DS3H_CTRL1_XL, databuf, 0x01)) {
		pr_info("[accel] read lsm6ds3h power ctl register err!\n");
		return LSM6DS3H_ERR_I2C;
	}
	pr_debug("[accel] LSM6DS3H_CTRL1_XL:databuf[0] =  %x!\n", databuf[0]);


	if (true == enable) {
		/*clear lsm6ds3h  ODR bits*/
		databuf[0] &= ~LSM6DS3H_ACC_ODR_MASK;
		/*LSM6DS3H_ACC_ODR_104HZ; //default set 100HZ for LSM6DS3H acc*/
		databuf[0] |= obj->sample_rate;
	} else {
		/* do nothing*/
		/*clear lsm6ds3h acc ODR bits*/
		databuf[0] &= ~LSM6DS3H_ACC_ODR_MASK;
		databuf[0] |= LSM6DS3H_ACC_ODR_POWER_DOWN;
	}
	/*databuf[1] = databuf[0];*/
	/*databuf[0] = LSM6DS3H_CTRL1_XL;*/
	res = mpu_i2c_write_block(client, LSM6DS3H_CTRL1_XL, databuf, 0x1);
	if (res < 0) {
		pr_debug("[accel] LSM6DS3H set power mode: ODR 100hz failed!\n");
		return LSM6DS3H_ERR_I2C;
	}
	pr_debug("[accel] set LSM6DS3H  power mode:ODR 100HZ ok %d!\n", enable);

	sensor_power = enable;

	return LSM6DS3H_SUCCESS;
}


/*----------------------------------------------------------------------------*/
static int LSM6DS3H_SetFullScale(struct i2c_client *client, u8 acc_fs)
{
	u8 databuf[2] = { 0 };
	int res = 0;
	struct lsm6ds3h_i2c_data *obj = i2c_get_clientdata(client);

	GSE_FUN();

	if (mpu_i2c_read_block(client, LSM6DS3H_CTRL1_XL, databuf, 0x01)) {
		pr_info("[accel] read LSM6DS3H_CTRL1_XL err!\n");
		return LSM6DS3H_ERR_I2C;
	}
	pr_debug("[accel] read  LSM6DS3H_CTRL1_XL register: 0x%x\n",
		databuf[0]);

	databuf[0] &= ~LSM6DS3H_ACC_RANGE_MASK;	/*clear*/
	databuf[0] |= acc_fs;

	/*databuf[1] = databuf[0];*/
	/*databuf[0] = LSM6DS3H_CTRL1_XL;*/

	res = mpu_i2c_write_block(client, LSM6DS3H_CTRL1_XL, databuf, 0x1);
	if (res < 0) {
		pr_info("[accel] write full scale register err!\n");
		return LSM6DS3H_ERR_I2C;
	}
	switch (acc_fs) {
	case LSM6DS3H_ACC_RANGE_2g:
		obj->sensitivity = LSM6DS3H_ACC_SENSITIVITY_2G;
		break;
	case LSM6DS3H_ACC_RANGE_4g:
		obj->sensitivity = LSM6DS3H_ACC_SENSITIVITY_4G;
		break;
	case LSM6DS3H_ACC_RANGE_8g:
		obj->sensitivity = LSM6DS3H_ACC_SENSITIVITY_8G;
		break;
	case LSM6DS3H_ACC_RANGE_16g:
		obj->sensitivity = LSM6DS3H_ACC_SENSITIVITY_16G;
		break;
	default:
		obj->sensitivity = LSM6DS3H_ACC_SENSITIVITY_2G;
		break;
	}

	if (mpu_i2c_read_block(client, LSM6DS3H_CTRL9_XL, databuf, 0x01)) {
		pr_info("[accel] read LSM6DS3H_CTRL9_XL err!\n");
		return LSM6DS3H_ERR_I2C;
	}
	pr_debug("[accel] read  LSM6DS3H_CTRL9_XL register: 0x%x\n",
		databuf[0]);

	databuf[0] &= ~LSM6DS3H_ACC_ENABLE_AXIS_MASK;	/*clear*/
	databuf[0] |=
	    LSM6DS3H_ACC_ENABLE_AXIS_X
	    | LSM6DS3H_ACC_ENABLE_AXIS_Y | LSM6DS3H_ACC_ENABLE_AXIS_Z;

	/*databuf[1] = databuf[0];*/
	/*databuf[0] = LSM6DS3H_CTRL9_XL;*/

	res = mpu_i2c_write_block(client, LSM6DS3H_CTRL9_XL, databuf, 0x1);
	if (res < 0) {
		pr_info("[accel] write full scale register err!\n");
		return LSM6DS3H_ERR_I2C;
	}

	return LSM6DS3H_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/* set the acc sample rate*/
static int LSM6DS3H_SetSampleRate(struct i2c_client *client, u8 sample_rate)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	GSE_FUN();
	/*set Sample Rate will enable power and should changed power status*/
	res = LSM6DS3H_SetPowerMode(client, true);
	if (res != LSM6DS3H_SUCCESS)
		return res;

	if (mpu_i2c_read_block(client, LSM6DS3H_CTRL1_XL, databuf, 0x01)) {
		pr_info("[accel] read acc data format register err!\n");
		return LSM6DS3H_ERR_I2C;
	}
	pr_debug("[accel] read  acc data format register: 0x%x\n", databuf[0]);

	databuf[0] &= ~LSM6DS3H_ACC_ODR_MASK;	/*clear*/
	databuf[0] |= sample_rate;

	/*databuf[1] = databuf[0];*/
	/*databuf[0] = LSM6DS3H_CTRL1_XL;*/

	res = mpu_i2c_write_block(client, LSM6DS3H_CTRL1_XL, databuf, 0x1);
	if (res < 0) {
		pr_info("[accel] write sample rate register err!\n");
		return LSM6DS3H_ERR_I2C;
	}

	return LSM6DS3H_SUCCESS;
}

#if 0
static int LSM6DS3H_Int_Ctrl(struct i2c_client *client,
	LSM6DS3H_ACC_GYRO_INT_ACTIVE_t int_act,
	LSM6DS3H_ACC_GYRO_INT_LATCH_CTL_t int_latch)
{
	u8 databuf[2] = { 0 };
	int res = 0;
	u8 op_reg = 0;

	GSE_FUN();

	/*config latch int or no latch*/
	op_reg = LSM6DS3H_TAP_CFG;
	if (mpu_i2c_read_block(client, op_reg, databuf, 0x01)) {
		pr_info("[accel] %s read data format register err!\n",
			__func__);
		return LSM6DS3H_ERR_I2C;
	}
	pr_debug("[accel] read  acc data format register: 0x%x\n", databuf[0]);

	databuf[0] &= ~LSM6DS3H_ACC_GYRO_INT_LATCH_CTL_MASK;	/*clear*/
	databuf[0] |= int_latch;

	/*databuf[1] = databuf[0];*/
	/*databuf[0] = op_reg;*/
	res = mpu_i2c_write_block(client, op_reg, databuf, 0x01);
	if (res < 0) {
		pr_info("[accel] write enable tilt func register err!\n");
		return LSM6DS3H_ERR_I2C;
	}
	/* config high or low active*/
	op_reg = LSM6DS3H_CTRL3_C;
	if (mpu_i2c_read_block(client, op_reg, databuf, 0x01)) {
		pr_info("[accel] %s read data format register err!\n",
			__func__);
		return LSM6DS3H_ERR_I2C;
	}
	pr_debug("[accel] read  acc data format register: 0x%x\n", databuf[0]);

	databuf[0] &= ~LSM6DS3H_ACC_GYRO_INT_ACTIVE_MASK;	/*clear*/
	databuf[0] |= int_act;

	/*databuf[1] = databuf[0];*/
	/*databuf[0] = op_reg;*/
	res = mpu_i2c_write_block(client, op_reg, databuf, 0x1);
	if (res < 0) {
		pr_info("[accel] write enable tilt func register err!\n");
		return LSM6DS3H_ERR_I2C;
	}

	return LSM6DS3H_SUCCESS;
}

static int LSM6DS3H_Enable_Func(struct i2c_client *client,
	LSM6DS3H_ACC_GYRO_FUNC_EN_t newValue)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	GSE_FUN();

	if (mpu_i2c_read_block(client, LSM6DS3H_CTRL10_C, databuf, 0x01)) {
		pr_info("[accel] %s read LSM6DS3H_CTRL10_C register err!\n",
			__func__);
		return LSM6DS3H_ERR_I2C;
	}
	pr_debug("[accel] %s read acc data format register: 0x%x\n",
		__func__, databuf[0]);
	databuf[0] &= ~LSM6DS3H_ACC_GYRO_FUNC_EN_MASK;	/*clear*/
	databuf[0] |= newValue;

	/*databuf[1] = databuf[0];*/
	/*databuf[0] = LSM6DS3H_CTRL10_C;*/
	res = mpu_i2c_write_block(client, LSM6DS3H_CTRL10_C, databuf, 0x01);
	if (res < 0) {
		pr_info("[accel] %s write LSM6DS3H_CTRL10_C register err!\n",
			__func__);
		return LSM6DS3H_ERR_I2C;
	}

	return LSM6DS3H_SUCCESS;
}
#endif

s16 LSM6DS3H_acc_TransfromResolution(s16 rawData, int sensitivity)
{
	s64 tempValue;
	uint64_t tempPlusValue = 0;

	tempValue = (s64) (rawData) * sensitivity * GRAVITY_EARTH_1000;
	if (tempValue < 0) {
		tempPlusValue = tempValue * -1;
		do_div(tempPlusValue, (1000 * 1000));
		tempValue = tempPlusValue * -1;
	} else {
		tempPlusValue = tempValue;
		do_div(tempPlusValue, (1000 * 1000));
		tempValue = tempPlusValue;
	}

	return (s16) tempValue;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3H_ReadAccData(struct i2c_client *client, char *buf,
	int bufsize)
{
	struct lsm6ds3h_i2c_data *obj =
		(struct lsm6ds3h_i2c_data *)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[LSM6DS3H_AXES_NUM];
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);

	if (buf == NULL)
		return -1;
	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	if (sensor_power == false) {
		res = LSM6DS3H_SetPowerMode(client, true);
		if (res)
			pr_info("[accel] Power on lsm6ds3h error %d!\n", res);
		msleep(20);
	}

	res = LSM6DS3H_ReadAccRawData(client, obj->data);
	if (res < 0) {
		pr_info("[accel] I2C error: ret value=%d", res);
		return -3;
	}
#if 1
	obj->data[LSM6DS3H_AXIS_X] =
	    LSM6DS3H_acc_TransfromResolution(obj->data[LSM6DS3H_AXIS_X]
					     , obj->sensitivity);
	obj->data[LSM6DS3H_AXIS_Y] =
	    LSM6DS3H_acc_TransfromResolution(obj->data[LSM6DS3H_AXIS_Y]
					     , obj->sensitivity);
	obj->data[LSM6DS3H_AXIS_Z] =
	    LSM6DS3H_acc_TransfromResolution(obj->data[LSM6DS3H_AXIS_Z]
					     , obj->sensitivity);

	obj->data[LSM6DS3H_AXIS_X] += obj->cali_sw[LSM6DS3H_AXIS_X];
	obj->data[LSM6DS3H_AXIS_Y] += obj->cali_sw[LSM6DS3H_AXIS_Y];
	obj->data[LSM6DS3H_AXIS_Z] += obj->cali_sw[LSM6DS3H_AXIS_Z];

	/*remap coordinate */
	acc[obj->cvt.map[LSM6DS3H_AXIS_X]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_X] * obj->data[LSM6DS3H_AXIS_X];
	acc[obj->cvt.map[LSM6DS3H_AXIS_Y]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Y] * obj->data[LSM6DS3H_AXIS_Y];
	acc[obj->cvt.map[LSM6DS3H_AXIS_Z]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Z] * obj->data[LSM6DS3H_AXIS_Z];


/*Out put the mg*/
/*
 *  acc[LSM6DS3H_AXIS_X] = acc[LSM6DS3H_AXIS_X] *
 * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
 *  acc[LSM6DS3H_AXIS_Y] = acc[LSM6DS3H_AXIS_Y] *
 * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
 *  acc[LSM6DS3H_AXIS_Z] = acc[LSM6DS3H_AXIS_Z] *
 * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
 */
#endif


	sprintf(buf, "%04x %04x %04x",
		acc[LSM6DS3H_AXIS_X], acc[LSM6DS3H_AXIS_Y],
		acc[LSM6DS3H_AXIS_Z]);
	/*atomic_read(&obj->trace) & ADX_TRC_IOCTL*/
	if (atomic_read(&obj->trace) & ADX_TRC_IOCTL) {
		/*pr_debug("[accel] gsensor data: %s!\n", buf);*/
		pr_debug("[accel] raw data:obj->data:%04x %04x %04x\n",
			obj->data[LSM6DS3H_AXIS_X],
			obj->data[LSM6DS3H_AXIS_Y], obj->data[LSM6DS3H_AXIS_Z]);
		pr_debug("[accel] acc:%04x %04x %04x\n",
			acc[LSM6DS3H_AXIS_X], acc[LSM6DS3H_AXIS_Y],
			acc[LSM6DS3H_AXIS_Z]);

		/*LSM6DS3H_dumpReg(client);*/
	}

	return 0;
}

static int LSM6DS3H_ReadAccRawData(struct i2c_client *client,
	s16 data[LSM6DS3H_AXES_NUM])
{
	int err = 0;
	char databuf[6] = { 0 };

	if (client == NULL)
		err = -EINVAL;
	else {
		if (hwmsen_read_block(client, LSM6DS3H_OUTX_L_XL, databuf, 6)) {
			pr_info("[accel] LSM6DS3H read acc data  error\n");
			return -2;
		}
		data[LSM6DS3H_AXIS_X] =
		    (s16) ((databuf[LSM6DS3H_AXIS_X * 2 + 1] << 8) |
			   (databuf[LSM6DS3H_AXIS_X * 2]));
		data[LSM6DS3H_AXIS_Y] =
		    (s16) ((databuf[LSM6DS3H_AXIS_Y * 2 + 1] << 8) |
			   (databuf[LSM6DS3H_AXIS_Y * 2]));
		data[LSM6DS3H_AXIS_Z] =
		    (s16) ((databuf[LSM6DS3H_AXIS_Z * 2 + 1] << 8) |
			   (databuf[LSM6DS3H_AXIS_Z * 2]));
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3H_ReadChipInfo(struct i2c_client *client, char *buf,
	int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8) * 10);

	if ((buf == NULL) || (bufsize <= 30))
		return -1;

	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "LSM6DS3H Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3h_acc_i2c_client;
	char strbuf[LSM6DS3H_BUFSIZE];

	if (client == NULL) {
		pr_info("[accel] i2c client is null!!\n");
		return 0;
	}

	LSM6DS3H_ReadChipInfo(client, strbuf, LSM6DS3H_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3h_acc_i2c_client;
	char strbuf[LSM6DS3H_BUFSIZE];
	int x, y, z;

	if (client == NULL) {
		pr_info("[accel] i2c client is null!!\n");
		return 0;
	}

	LSM6DS3H_ReadAccData(client, strbuf, LSM6DS3H_BUFSIZE);
	if (sscanf(strbuf, "%x %x %x", &x, &y, &z) != 3)
		pr_debug("[accel] get data format error\n");
	return snprintf(buf, PAGE_SIZE, "%d, %d, %d\n", x, y, z);
}

static ssize_t show_sensorrawdata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3h_acc_i2c_client;
	s16 data[LSM6DS3H_AXES_NUM] = { 0 };

	if (client == NULL) {
		pr_info("[accel] i2c client is null!!\n");
		return 0;
	}

	LSM6DS3H_ReadAccRawData(client, data);
	return snprintf(buf, PAGE_SIZE, "%x,%x,%x\n",
		data[0], data[1], data[2]);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct lsm6ds3h_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_info("[accel] i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf,
	size_t count)
{
	struct lsm6ds3h_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		pr_info("[accel] i2c_data obj is null!!\n");
		return count;
	}

	if (kstrtos32(buf, 10, &trace) == 0)
		atomic_set(&obj->trace, trace);
	else
		pr_info("[accel] invalid content: '%s', length = %zu\n",
			buf, count);

	return count;
}

static ssize_t show_chipinit_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct lsm6ds3h_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_info("[accel] i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_chipinit_value(struct device_driver *ddri,
	const char *buf, size_t count)
{
	struct lsm6ds3h_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_info("[accel] i2c_data obj is null!!\n");
		return count;
	}

	LSM6DS3H_init_client(obj->client, true);
	LSM6DS3H_dumpReg(obj->client);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct lsm6ds3h_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_info("[accel] i2c_data obj is null!!\n");
		return 0;
	}

	if (obj->hw) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"CUST: i2c_num=%d, direction=%d, sensitivity = %d,(power_id=%d, power_vol=%d)\n",
			obj->hw->i2c_num, obj->hw->direction, obj->sensitivity,
			obj->hw->power_id, obj->hw->power_vol);
		LSM6DS3H_dumpReg(obj->client);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}
	return len;
}

static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct lsm6ds3h_i2c_data *data = obj_i2c_data;

	if (data == NULL) {
		pr_info("[accel] lsm6ds3h_i2c_data is null!!\n");
		return -1;
	}

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction, atomic_read(&data->layout),
		data->cvt.sign[0],
		data->cvt.sign[1], data->cvt.sign[2], data->cvt.map[0],
		data->cvt.map[1],
		data->cvt.map[2]);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri,
	const char *buf, size_t count)
{
	int layout = 0;
	struct lsm6ds3h_i2c_data *data = obj_i2c_data;
	int ret = 0;

	if (data == NULL) {
		pr_info("[accel] lsm6ds3h_i2c_data is null!!\n");
		return count;
	}

	if (kstrtos32(buf, 10, &layout) == 0) {
		atomic_set(&data->layout, layout);
		if (!hwmsen_get_convert(layout, &data->cvt)) {
			pr_info("[accel] HWMSEN_GET_CONVERT function error!\r\n");
		} else if (!hwmsen_get_convert(data->hw->direction,
				&data->cvt)) {
			pr_info("[accel] invalid layout: %d, restore to %d\n",
				layout, data->hw->direction);
		} else {
			pr_info("[accel] invalid layout: (%d, %d)\n",
				layout, data->hw->direction);
			ret = hwmsen_get_convert(0, &data->cvt);
			if (!ret)
				pr_info("[accel] HWMSEN_GET_CONVERT function error!\r\n");
		}
	} else {
		pr_info("[accel] invalid format = '%s'\n", buf);
	}

	return count;
}

/*----------------------------------------------------------------------------*/

static DRIVER_ATTR(chipinfo, 0444, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensorrawdata, 0444, show_sensorrawdata_value, NULL);
static DRIVER_ATTR(sensordata, 0444, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, 0644,	show_trace_value, store_trace_value);
static DRIVER_ATTR(chipinit, 0644, show_chipinit_value, store_chipinit_value);
static DRIVER_ATTR(status, 0444, show_status_value, NULL);
static DRIVER_ATTR(layout, 0644, show_layout_value, store_layout_value);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *LSM6DS3H_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_sensorrawdata,	/*dump sensor raw data */
	&driver_attr_trace,	/*trace log */
	&driver_attr_status,
	&driver_attr_chipinit,
	&driver_attr_layout,
};

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(LSM6DS3H_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, LSM6DS3H_attr_list[idx]);
		if (err != 0) {
			pr_info("[accel] driver_create_file (%s) = %d\n",
				LSM6DS3H_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(LSM6DS3H_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, LSM6DS3H_attr_list[idx]);
	return err;
}

static int LSM6DS3H_Set_RegInc(struct i2c_client *client, bool inc)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	if (mpu_i2c_read_block(client, LSM6DS3H_CTRL3_C, databuf, 0x01)) {
		pr_info("[accel] read LSM6DS3H_CTRL3_XL err!\n");
		return LSM6DS3H_ERR_I2C;
	}
	pr_debug("[accel] read  LSM6DS3H_CTRL3_C register: 0x%x\n", databuf[0]);

	if (inc) {
		databuf[0] |= LSM6DS3H_CTRL3_C_IFINC;

		/*databuf[1] = databuf[0];*/
		/*databuf[0] = LSM6DS3H_CTRL3_C;*/
		res = mpu_i2c_write_block(client, LSM6DS3H_CTRL3_C,
					databuf, 0x1);
		if (res < 0) {
			pr_info("[accel] write full scale register err!\n");
			return LSM6DS3H_ERR_I2C;
		}
	}

	return LSM6DS3H_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3H_init_client(struct i2c_client *client, bool enable)
{
	struct lsm6ds3h_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	GSE_FUN();
	pr_debug("[accel]  lsm6ds3h addr %x!\n", client->addr);
	res = LSM6DS3H_CheckDeviceID(client);
	if (res != LSM6DS3H_SUCCESS)
		return res;

	res = LSM6DS3H_Set_RegInc(client, true);
	if (res != LSM6DS3H_SUCCESS)
		return res;
	/*we have only this choice*/
	res = LSM6DS3H_SetFullScale(client, LSM6DS3H_ACC_RANGE_2g);
	if (res != LSM6DS3H_SUCCESS)
		return res;
	res = LSM6DS3H_SetSampleRate(client, obj->sample_rate);
	if (res != LSM6DS3H_SUCCESS)
		return res;


	res = LSM6DS3H_SetPowerMode(client, enable);
	if (res != LSM6DS3H_SUCCESS)
		return res;

	pr_debug("[accel] %s OK!\n", __func__);
	/*acc setting*/

#ifdef CONFIG_LSM6DS3H_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	return LSM6DS3H_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_open_report_data(int open)
{
	/*should queuq work to report event if  is_report_input_direct=true*/
	return 0;
}

/* if use this typ of enable ,
 * Gsensor only enabled but not report inputEvent to HAL
 */
static int lsm6ds3h_enable_nodata(int en)
{
	int value = en;
	int err = 0;
	struct lsm6ds3h_i2c_data *priv = obj_i2c_data;

	if (priv == NULL) {
		pr_info("[accel] obj_i2c_data is NULL!\n");
		return -1;
	}

	if (value == 1) {
		enable_status = true;
	} else {
		enable_status = false;
		priv->sample_rate = LSM6DS3H_ACC_ODR_104HZ; /*default rate*/
	}
	pr_debug("[accel] enable value=%d, sensor_power =%d\n",
		value, sensor_power);

	if (((value == 0) && (sensor_power == false)) || ((value == 1)
		&& (sensor_power == true)))
		pr_debug("[accel] Gsensor device have updated!\n");
	else
		err = LSM6DS3H_SetPowerMode(priv->client, enable_status);


	pr_debug("[accel] %s OK!\n", __func__);
	return err;
}

static int lsm6ds3h_set_delay(u64 ns)
{
	int value = 0;
	int err = 0;
	u8 sample_delay = 0;
	struct lsm6ds3h_i2c_data *priv = obj_i2c_data;

	value = (int)ns / 1000 / 1000;

	if (priv == NULL) {
		pr_info("[accel] obj_i2c_data is NULL!\n");
		return -1;
	}

	if (value <= 5)
		sample_delay = LSM6DS3H_ACC_ODR_208HZ;
	else if (value <= 10)
		sample_delay = LSM6DS3H_ACC_ODR_104HZ;
	else
		sample_delay = LSM6DS3H_ACC_ODR_52HZ;

	priv->sample_rate = sample_delay;
	err = LSM6DS3H_SetSampleRate(priv->client, sample_delay);
	if (err != LSM6DS3H_SUCCESS)
		pr_info("[accel] Set delay parameter error!\n");


	if (value >= 50)
		atomic_set(&priv->filter, 0);
	else {
		priv->fir.num = 0;
		priv->fir.idx = 0;
		priv->fir.sum[LSM6DS3H_AXIS_X] = 0;
		priv->fir.sum[LSM6DS3H_AXIS_Y] = 0;
		priv->fir.sum[LSM6DS3H_AXIS_Z] = 0;
		atomic_set(&priv->filter, 1);
	}

	pr_debug("[accel] %s (%d), chip only use 1024HZ\n", __func__, value);
	return 0;
}

static int gsensor_batch(int flag, int64_t samplingPeriodNs,
	int64_t maxBatchReportLatencyNs)
{
	int value = 0;

	value = (int)samplingPeriodNs / 1000 / 1000;

	pr_debug("[accel] lsm6ds3h acc set delay = (%d) ok.\n", value);
	return lsm6ds3h_set_delay(samplingPeriodNs);
}

static int gsensor_flush(void)
{
	return acc_flush_report();
}

static int lsm6ds3h_get_data(int *x, int *y, int *z, int *status)
{
	char buff[LSM6DS3H_BUFSIZE];
	struct lsm6ds3h_i2c_data *priv = obj_i2c_data;

	if (priv == NULL) {
		pr_info("[accel] obj_i2c_data is NULL!\n");
		return -1;
	}
	if (atomic_read(&priv->trace) & ACCEL_TRC_DATA)
		pr_debug("[accel] %s (%d),\n", __func__, __LINE__);

	memset(buff, 0, sizeof(buff));
	LSM6DS3H_ReadAccData(priv->client, buff, LSM6DS3H_BUFSIZE);

	if (sscanf(buff, "%x %x %x", x, y, z) != 3)
		pr_debug("[accel] get data format error\n");
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

#if 0				/*ioctl related */
/******************************************************************************
 * Function Configuration
 ******************************************************************************/
static int lsm6ds3h_open(struct inode *inode, struct file *file)
{
	file->private_data = lsm6ds3h_acc_i2c_client;

	if (file->private_data == NULL) {
		pr_info("[accel] null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
static long lsm6ds3h_unlocked_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct lsm6ds3h_i2c_data *obj =
		(struct lsm6ds3h_i2c_data *)i2c_get_clientdata(client);
	char strbuf[LSM6DS3H_BUFSIZE];
	void __user *data;
	struct SENSOR_DATA sensor_data;
	int err = 0;
	int cali[3];

	/*GSE_FUN(f);*/
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
			_IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
			_IOC_SIZE(cmd));


	if (err) {
		pr_info("[accel] access error: %08X, (%2d, %2d)\n",
			cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GSENSOR_IOCTL_INIT:
		break;

	case GSENSOR_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		LSM6DS3H_ReadChipInfo(client, strbuf, LSM6DS3H_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_SENSORDATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		LSM6DS3H_ReadAccData(client, strbuf, LSM6DS3H_BUFSIZE);

		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_GAIN:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		break;

	case GSENSOR_IOCTL_READ_OFFSET:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		break;

	case GSENSOR_IOCTL_READ_RAW_DATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		LSM6DS3H_ReadAccRawData(client, (s16 *) strbuf);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_SET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		if (atomic_read(&obj->suspend)) {
			pr_info("[accel] Perform calibration in suspend state!!\n");
			err = -EINVAL;
		} else {
#if 0
			cali[LSM6DS3H_AXIS_X] =
			    (s64) (sensor_data.x) * 1000 * 1000
			    / (obj->sensitivity * GRAVITY_EARTH_1000);	/*NTC*/
			cali[LSM6DS3H_AXIS_Y] =
			    (s64) (sensor_data.y) * 1000 * 1000
			    / (obj->sensitivity * GRAVITY_EARTH_1000);
			cali[LSM6DS3H_AXIS_Z] =
			    (s64) (sensor_data.z) * 1000 * 1000
			    / (obj->sensitivity * GRAVITY_EARTH_1000);
#else
			cali[LSM6DS3H_AXIS_X] = (s64) (sensor_data.x);
			cali[LSM6DS3H_AXIS_Y] = (s64) (sensor_data.y);
			cali[LSM6DS3H_AXIS_Z] = (s64) (sensor_data.z);
#endif
			err = LSM6DS3H_WriteCalibration(client, cali);
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		err = LSM6DS3H_ResetCalibration(client);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = LSM6DS3H_ReadCalibration(client, cali);
		if (err < 0)
			break;
#if 0
		sensor_data.x =
		    (s64) (cali[LSM6DS3H_AXIS_X]) * obj->sensitivity
		    * GRAVITY_EARTH_1000 / (1000 * 1000);	/*NTC*/
		sensor_data.y =
		    (s64) (cali[LSM6DS3H_AXIS_Y]) * obj->sensitivity
		    * GRAVITY_EARTH_1000 / (1000 * 1000);
		sensor_data.z =
		    (s64) (cali[LSM6DS3H_AXIS_Z]) * obj->sensitivity
		    * GRAVITY_EARTH_1000 / (1000 * 1000);
#else
		sensor_data.x = (s64) (cali[LSM6DS3H_AXIS_X]);
		sensor_data.y = (s64) (cali[LSM6DS3H_AXIS_Y]);
		sensor_data.z = (s64) (cali[LSM6DS3H_AXIS_Z]);
#endif
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		pr_info("[accel] unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}

#ifdef CONFIG_COMPAT
static long lsm6ds3h_compat_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	long err = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_GSENSOR_IOCTL_READ_SENSORDATA:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file,
				GSENSOR_IOCTL_READ_SENSORDATA,
				(unsigned long)arg32);
		if (err) {
			pr_info("[accel] GSENSOR_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
			return err;
		}
		break;

	case COMPAT_GSENSOR_IOCTL_SET_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_SET_CALI,
				(unsigned long)arg32);
		if (err) {
			pr_info("[accel] GSENSOR_IOCTL_SET_CALI unlocked_ioctl failed.");
			return err;
		}
		break;

	case COMPAT_GSENSOR_IOCTL_GET_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_GET_CALI,
				(unsigned long)arg32);
		if (err) {
			pr_info("[accel] GSENSOR_IOCTL_GET_CALI unlocked_ioctl failed.");
			return err;
		}
		break;

	case COMPAT_GSENSOR_IOCTL_CLR_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_CLR_CALI,
				(unsigned long)arg32);
		if (err) {
			pr_info("[accel] GSENSOR_IOCTL_CLR_CALI unlocked_ioctl failed.");
			return err;
		}
		break;

	default:
		pr_info("[accel] unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}

	return err;
}
#endif

/*----------------------------------------------------------------------------*/
static const struct file_operations lsm6ds3h_acc_fops = {
	.owner = THIS_MODULE,
	.open = lsm6ds3h_open,
	.release = lsm6ds3h_release,
	.unlocked_ioctl = lsm6ds3h_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = lsm6ds3h_compat_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice lsm6ds3h_acc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &lsm6ds3h_acc_fops,
};
#endif				/*ioctl related */
/*----------------------------------------------------------------------------*/

static int bmi160_factory_enable_sensor(bool enabledisable,
	int64_t sample_periods_ms)
{
	int err;
#if 0
	err = bmi160_acc_enable_nodata(enabledisable == true ? 1 : 0);
	if (err) {
		pr_info("[accel] %s enable sensor failed!\n", __func__);
		return -1;
	}
	err = bmi160_acc_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		pr_info("[accel] %s enable set batch failed!\n", __func__);
		return -1;
	}
#endif
	err = LSM6DS3H_init_client(lsm6ds3h_acc_i2c_client, 0);
	if (err) {
		pr_info("[accel] initialize client fail!!\n");
		return -1;
	}
	return 0;
}

static int bmi160_factory_get_data(int32_t data[3], int *status)
{
	int err = 0;
#if 0
	return bmi160_acc_get_data(&data[0], &data[1], &data[2], status);
#endif
	mutex_lock(&lsm6ds3h_factory_mutex);
	if (sensor_power == false) {
		err = LSM6DS3H_SetPowerMode(lsm6ds3h_acc_i2c_client, true);
		if (err)
			pr_info("[accel] Power on lsm6ds3h error %d!\n", err);

		msleep(50);
	}
	mutex_unlock(&lsm6ds3h_factory_mutex);

	return lsm6ds3h_get_data(&data[0], &data[1], &data[2], status);
}

static int bmi160_factory_get_raw_data(int32_t data[3])
{
	s16 strbuf[3] = {0};

	LSM6DS3H_ReadAccRawData(lsm6ds3h_acc_i2c_client, strbuf);
	data[0] = strbuf[0];
	data[1] = strbuf[1];
	data[2] = strbuf[2];

	return 0;
}

static int bmi160_factory_enable_calibration(void)
{
	return 0;
}

static int bmi160_factory_clear_cali(void)
{
	int err = 0;

	/* err = BMI160_ACC_ResetCalibration(obj_data); */
	err = LSM6DS3H_ResetCalibration(lsm6ds3h_acc_i2c_client);
	if (err) {
		pr_info("[accel] lsm6ds3h_ResetCalibration failed!\n");
		return -1;
	}
	return 0;
}

static int bmi160_factory_set_cali(int32_t data[3])
{
	int err = 0;
	int cali[3] = { 0 };
#if 0
	cali[BMI160_ACC_AXIS_X] = data[0]
	    * obj_data->reso->sensitivity / GRAVITY_EARTH_1000;
	cali[BMI160_ACC_AXIS_Y] = data[1]
	    * obj_data->reso->sensitivity / GRAVITY_EARTH_1000;
	cali[BMI160_ACC_AXIS_Z] = data[2]
	    * obj_data->reso->sensitivity / GRAVITY_EARTH_1000;
	err = BMI160_ACC_WriteCalibration(obj_data, cali);
#endif
	cali[LSM6DS3H_AXIS_X] = data[0];
	cali[LSM6DS3H_AXIS_Y] = data[1];
	cali[LSM6DS3H_AXIS_Z] = data[2];
	err = LSM6DS3H_WriteCalibration(lsm6ds3h_acc_i2c_client, cali);
	if (err) {
		pr_info("[accel] LSM6DS3H_WriteCalibration failed!\n");
		return -1;
	}
	return 0;
}

static int bmi160_factory_get_cali(int32_t data[3])
{
	int err = 0;
	int cali[3] = { 0 };
#if 0
	err = BMI160_ACC_ReadCalibration(obj_data, cali);
	if (err) {
		pr_info("[accel] bmi160_ReadCalibration failed!\n");
		return -1;
	}
	data[0] = cali[BMI160_ACC_AXIS_X]
	    * GRAVITY_EARTH_1000 / obj_data->reso->sensitivity;
	data[1] = cali[BMI160_ACC_AXIS_Y]
	    * GRAVITY_EARTH_1000 / obj_data->reso->sensitivity;
	data[2] = cali[BMI160_ACC_AXIS_Z]
	    * GRAVITY_EARTH_1000 / obj_data->reso->sensitivity;
#endif
	err = LSM6DS3H_ReadCalibration(lsm6ds3h_acc_i2c_client, cali);
	if (err) {
		pr_info("[accel] LSM6DS3H_ReadCalibration failed!\n");
		return -1;
	}
	data[0] = cali[LSM6DS3H_AXIS_X];
	data[1] = cali[LSM6DS3H_AXIS_Y];
	data[2] = cali[LSM6DS3H_AXIS_Z];
	return 0;
}

static int bmi160_factory_do_self_test(void)
{
	return 0;
}

static struct accel_factory_fops lsm6ds3ha_factory_fops = {
	.enable_sensor = bmi160_factory_enable_sensor,
	.get_data = bmi160_factory_get_data,
	.get_raw_data = bmi160_factory_get_raw_data,
	.enable_calibration = bmi160_factory_enable_calibration,
	.clear_cali = bmi160_factory_clear_cali,
	.set_cali = bmi160_factory_set_cali,
	.get_cali = bmi160_factory_get_cali,
	.do_self_test = bmi160_factory_do_self_test,
};

static struct accel_factory_public lsm6ds3ha_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &lsm6ds3ha_factory_fops,
};

/*----------------------------------------------------------------------------*/

#ifdef CONFIG_PM_SLEEP
static int lsm6ds3h_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lsm6ds3h_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	if (obj == NULL) {
		pr_info("[accel] null pointer!!\n");
		return -EINVAL;
	}
	atomic_set(&obj->suspend, 1);

	err = LSM6DS3H_SetPowerMode(obj->client, false);
	if (err) {
		pr_info("[accel] write power control fail!!\n");
		return err;
	}

	sensor_power = false;

	LSM6DS3H_power(obj->hw, 0);

	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lsm6ds3h_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	GSE_FUN();

	if (obj == NULL) {
		pr_info("[accel] null pointer!!\n");
		return -1;
	}

	LSM6DS3H_power(obj->hw, 1);
	err = LSM6DS3H_SetPowerMode(obj->client, enable_status);
	if (err) {
		pr_info("[accel] initialize client fail! err code %d!\n", err);
		return err;
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int lsm6ds3h_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct i2c_client *new_client = NULL;
	struct lsm6ds3h_i2c_data *obj = NULL;
	struct acc_control_path ctl_path = { 0 };
	struct acc_data_path data_path = { 0 };
	int err = 0;

	err = get_accel_dts_func(client->dev.of_node, hw);
	if (err) {
		pr_info("[accel] get dts info fail\n");
		err = -EFAULT;
		goto exit;
	}
	LSM6DS3H_power(hw, 1);

	/*GSE_FUN();*/
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(struct lsm6ds3h_i2c_data));

	obj->hw = hw;
	obj->sample_rate = LSM6DS3H_ACC_ODR_208HZ;	/*104HZ??*/

	/*atomic_set(&obj->layout, obj->hw->direction);*/
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		pr_info("[accel] invalid direction: %d\n", obj->hw->direction);
		goto exit_kfree;
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

	lsm6ds3h_acc_i2c_client = new_client;
	err = LSM6DS3H_init_client(new_client, false);	/*??*/
	if (err)
		goto exit_init_failed;


	/* err = misc_register(&lsm6ds3h_acc_device); */
	err = accel_factory_device_register(&lsm6ds3ha_factory_device);
	if (err) {
		pr_info("[accel] acc_factory register failed!\n");
		goto exit_misc_device_register_failed;
	}

	/*mutex_lock(&lsm6ds3h_init_mutex);*/

	ctl_path.is_use_common_factory = true;
	err = lsm6ds3h_create_attr(
		&(lsm6ds3h_init_info.platform_diver_addr->driver));
	if (err < 0)
		goto exit_create_attr_failed;


	ctl_path.open_report_data = lsm6ds3h_open_report_data;
	ctl_path.enable_nodata = lsm6ds3h_enable_nodata;
	/*ctl_path.set_delay  = lsm6ds3h_set_delay; */
	ctl_path.batch = gsensor_batch;
	ctl_path.flush = gsensor_flush;
	ctl_path.is_report_input_direct = false;
	ctl_path.is_support_batch = obj->hw->is_batch_supported;
	err = acc_register_control_path(&ctl_path);
	if (err) {
		pr_info("[accel] register acc control path err\n");
		goto exit_init_failed;
	}

	data_path.get_data = lsm6ds3h_get_data;
	data_path.vender_div = 1000;
	err = acc_register_data_path(&data_path);
	if (err) {
		pr_info("[accel] register acc data path err= %d\n", err);
		goto exit_init_failed;
	}
/*mutex_unlock(&lsm6ds3h_init_mutex);*/

	lsm6ds3h_init_flag = 0;
	pr_debug("[accel] %s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
	/*misc_deregister(&lsm6ds3h_acc_device); */
exit_misc_device_register_failed:
exit_init_failed:
exit_kfree:
	kfree(obj);
exit:
	lsm6ds3h_init_flag = -1;
	pr_info("[accel] %s: err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	LSM6DS3H_power(hw, 0);

	/*lsm6ds3h_init_flag = -1;*/
	err = lsm6ds3h_delete_attr(
		&(lsm6ds3h_init_info.platform_diver_addr->driver));
	if (err)
		pr_info("[accel] %s fail: %d\n", __func__, err);


	/*misc_deregister(&lsm6ds3h_acc_device); */
	accel_factory_device_deregister(&lsm6ds3ha_factory_device);

	lsm6ds3h_acc_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/*----------------------------------------------------------------------------*/

static int lsm6ds3h_local_uninit(void)
{
	/*GSE_FUN();*/
	LSM6DS3H_power(hw, 0);
	i2c_del_driver(&lsm6ds3h_i2c_driver);
	return 0;
}

static int lsm6ds3h_local_init(void)
{
	/*GSE_FUN();*/

#if 0				/* sensors-1.0 */
	LSM6DS3H_power(hw, 1);	/*??*/
#endif

	if (i2c_add_driver(&lsm6ds3h_i2c_driver)) {
		pr_info("[accel] add driver error\n");
		return -1;
	}
	if (lsm6ds3h_init_flag == -1) {
		pr_info("[accel] %s init failed!\n", __func__);
		return -1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init lsm6ds3h_init(void)
{
	acc_driver_add(&lsm6ds3h_init_info);

	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit lsm6ds3h_exit(void)
{
	GSE_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(lsm6ds3h_init);
module_exit(lsm6ds3h_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LSM6DS3H Accelerometer");
MODULE_AUTHOR("Yue.Wu@mediatek.com");
