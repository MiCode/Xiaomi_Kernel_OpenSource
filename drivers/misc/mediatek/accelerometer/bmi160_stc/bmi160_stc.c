/* BOSCH STEP COUNTER Sensor Driver

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

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <cust_acc.h>
#include <step_counter.h>

#include "bmi160_stc.h"

#include <step_counter.h>

extern struct i2c_client *bmi160_acc_i2c_client;

struct acc_hw acc_cust;
static struct acc_hw *hw = &acc_cust;

struct pedometer_data_t {
	u8 wkar_step_detector_status;
	u_int32_t last_step_counter_value;
};

/* bmg i2c client data */
struct step_c_i2c_data {
	struct i2c_client *client;
	u8 sensor_name[MAX_SENSOR_NAME];
	enum SENSOR_TYPE_ENUM sensor_type;
	enum STC_POWERMODE_ENUM power_mode;
	int datarate;
	struct mutex lock;
	atomic_t	trace;
	atomic_t	suspend;
	atomic_t	filter;
	struct pedometer_data_t pedo_data;
};

/* 0 = OK, -1 = fail */
static int step_c_init_flag =-1;
static struct i2c_driver step_c_i2c_driver;
static struct step_c_i2c_data *obj_i2c_data;
static int step_c_set_powermode(struct i2c_client *client,
		enum STC_POWERMODE_ENUM power_mode);

static const struct i2c_device_id step_c_i2c_id[] = {
	{STC_DEV_NAME, 0},
	{}
};

static int stc_i2c_read_block(struct i2c_client *client, u8 addr,
				u8 *data, u8 len)
{
	u8 beg = addr;
	int err;
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &beg
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,	.buf = data,
		}
	};
	if (!client)
		return -EINVAL;
	else if (len > C_I2C_FIFO_SIZE) {
		STEP_C_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
	if (err != 2) {
		STEP_C_ERR("i2c_transfer error: (%d %p %d) %d\n",
			addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	return err;
}

static int stc_i2c_write_block(struct i2c_client *client, u8 addr,
				u8 *data, u8 len)
{
	int err, idx = 0, num = 0;
	char buf[C_I2C_FIFO_SIZE];
	if (!client)
		return -EINVAL;
	else if (len >= C_I2C_FIFO_SIZE) {
		STEP_C_ERR("length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	buf[num++] = addr;
	for (idx = 0; idx < len; idx++) {
		buf[num++] = data[idx];
	}
	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		STEP_C_ERR("send command error!!\n");
		return -EFAULT;
	} else {
		err = 0;
	}
	return err;
}

static int step_c_get_chip_type(struct i2c_client *client)
{
	int err = 0;
	u8 chip_id = 0;
	struct step_c_i2c_data *obj = obj_i2c_data;
	err = stc_i2c_read_block(client, BMI160_USER_CHIP_ID__REG, &chip_id, 1);
	mdelay(20);
	if (err < 0) {
		STEP_C_ERR("read chip id failed.\n");
		return err;
	}
	switch (chip_id) {
	case SENSOR_CHIP_ID_BMI:
	case SENSOR_CHIP_ID_BMI_C2:
	case SENSOR_CHIP_ID_BMI_C3:
		obj->sensor_type = BMI160_STC_TYPE;
		strcpy(obj->sensor_name, SENSOR_NAME);
		break;
	default:
		obj->sensor_type = INVALID_TYPE;
		strcpy(obj->sensor_name, UNKNOWN_SENSOR);
		break;
	}
	if (obj->sensor_type == INVALID_TYPE) {
		STEP_C_ERR("unknown sensor.\n");
		return ERROR;
	}
	STEP_C_LOG("read sensor chip id = %d ok.\n", (int)chip_id);
	return SUCCESS;
}

static int step_c_set_powermode(struct i2c_client *client,
		enum STC_POWERMODE_ENUM power_mode)
{
	int err = 0;
	u8 actual_power_mode = 0;
	struct step_c_i2c_data *obj = obj_i2c_data;
	if (power_mode == obj->power_mode) {
		return err;
	}
	mutex_lock(&obj->lock);
	if (power_mode == STC_SUSPEND_MODE) {
		actual_power_mode = CMD_PMU_ACC_SUSPEND;
	} else {
		actual_power_mode = CMD_PMU_ACC_NORMAL;
	}
	err = stc_i2c_write_block(client,
		BMI160_CMD_COMMANDS__REG, &actual_power_mode, 1);
	mdelay(10);
	if (err < 0) {
		STEP_C_ERR("set power mode failed.\n");
		mutex_unlock(&obj->lock);
		return err;
	}
	obj->power_mode = power_mode;
	mutex_unlock(&obj->lock);
	STEP_C_LOG("set power mode = %d ok.\n", (int)actual_power_mode);
	return err;
}

static int step_c_set_datarate(struct i2c_client *client,
		int datarate)
{
	int err = 0;
	u8 data = 0;
	struct step_c_i2c_data *obj = obj_i2c_data;
	if (datarate == obj->datarate) {
		return 0;
	}
	mutex_lock(&obj->lock);
	if (obj->sensor_type == BMI160_STC_TYPE) {
		err = stc_i2c_read_block(client,
			BMI160_USER_ACC_CONF_ODR__REG, &data, 1);
		data = BMI160_SET_BITSLICE(data,
			BMI160_USER_ACC_CONF_ODR, datarate);
		err += stc_i2c_write_block(client,
			BMI160_USER_ACC_CONF_ODR__REG, &data, 1);
	}
	if (err < 0) {
		STEP_C_ERR("set data rate failed.\n");
	}else {
		obj->datarate = datarate;
	}
	mutex_unlock(&obj->lock);
	STEP_C_LOG("set data rate = %d ok.\n", (int)datarate);
	return err;
}


static int step_c_init_client(struct i2c_client *client)
{
	int err = 0;
	err = step_c_get_chip_type(client);
	if (err < 0) {
		return err;
	}
	err = step_c_set_datarate(client, BMI160_ACCEL_ODR_200HZ);
	if (err < 0) {
		return err;
	}
	err = step_c_set_powermode(client,
		(enum STC_POWERMODE_ENUM)STC_SUSPEND_MODE);
	if (err < 0) {
		return err;
	}
	return err;
}

static int step_c_open(struct inode *inode, struct file *file)
{
	file->private_data = obj_i2c_data;
	if (file->private_data == NULL) {
		STEP_C_ERR("file->private_data == NULL.\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int step_c_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long stc_c_unlocked_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	return 0;
}

static const struct file_operations step_c_fops = {
	.owner = THIS_MODULE,
	.open = step_c_open,
	.release = step_c_release,
	.unlocked_ioctl = stc_c_unlocked_ioctl,
};

static struct miscdevice step_c_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "step_counter",
	.fops = &step_c_fops,
};

static int step_c_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct step_c_i2c_data *obj = obj_i2c_data;
	int err = 0;
	STEP_C_FUN();
	if (msg.event == PM_EVENT_SUSPEND) {
		atomic_set(&obj->suspend, ENABLE);
		//err = step_c_set_powermode(obj->client, STC_SUSPEND_MODE);
		//if (err) {
		//	STEP_C_ERR("set step counter suspend mode failed.\n");
		//	return err;
		//}
	}
	return err;
}

static int step_c_resume(struct i2c_client *client)
{
	int err;
	struct step_c_i2c_data *obj = obj_i2c_data;
	err = step_c_init_client(client);
	if (err) {
		STEP_C_ERR("initialize client failed.\n");
		return err;
	}
	atomic_set(&obj->suspend, 0);
	return 0;
}

static int step_c_i2c_detect(struct i2c_client *client,
		struct i2c_board_info *info)
{
	strcpy(info->type, STC_DEV_NAME);
	return 0;
}

static int step_c_open_report_data(int open)
{
	return 0;
}

static int step_c_enable_nodata(int en)
{
	return 0;
}

static int step_c_set_delay(u64 ns)
{
	int err;
	int sample_delay = 0;
	int value = (int)ns/1000/1000 ;
	struct step_c_i2c_data *obj = obj_i2c_data;
	if(value <= 5) {
		sample_delay = BMI160_ACCEL_ODR_200HZ;
	}else if(value <= 10) {
		sample_delay = BMI160_ACCEL_ODR_100HZ;
	}else if(value <= 20) {
		sample_delay = BMI160_ACCEL_ODR_50HZ;
	}else if(value <= 40) {
		sample_delay = BMI160_ACCEL_ODR_25HZ;
	}else {
		sample_delay = BMI160_ACCEL_ODR_100HZ;
	}
	STEP_C_LOG("sensor delay value = %d, sample delay = %d\n",
			value, sample_delay);
	err = step_c_set_datarate(obj->client, sample_delay);
	if (err < 0)
		STEP_C_ERR("set data rate error.\n");
	return err;
}


/*!
*	@brief This API is used to write, select
*	the significant or any motion interrupt from the register 0x62 bit 1
*
*  @param  v_intr_significant_motion_select_u8 :
*	the value of significant or any motion interrupt selection
*	value    | Behaviour
* ----------|-------------------
*  0x00     |  ANY_MOTION
*  0x01     |  SIGNIFICANT_MOTION
*
*	@return results of bus communication function
*	@retval 0 -> Success
*	@retval -1 -> Error
*
*/
static BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_significant_motion_select(
u8 v_intr_significant_motion_select_u8)
{
	struct step_c_i2c_data *obj = obj_i2c_data;
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	if (v_intr_significant_motion_select_u8 <=
			BMI160_MAX_VALUE_SIGNIFICANT_MOTION) {
		/* write the significant or any motion interrupt*/
		com_rslt = stc_i2c_read_block(obj->client,
		BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT,
			v_intr_significant_motion_select_u8);
			com_rslt += stc_i2c_write_block(obj_i2c_data->client,
			BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
	}
	return com_rslt;
}

/*!
 *	@brief  This API is used to set
 *	interrupt enable from the register 0x50 bit 0 to 7
 *
 *	@param v_enable_u8 : Value to decided to select interrupt
 *   v_enable_u8   |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_ANY_MOTION_X_ENABLE
 *       1         | BMI160_ANY_MOTION_Y_ENABLE
 *       2         | BMI160_ANY_MOTION_Z_ENABLE
 *       3         | BMI160_DOUBLE_TAP_ENABLE
 *       4         | BMI160_SINGLE_TAP_ENABLE
 *       5         | BMI160_ORIENT_ENABLE
 *       6         | BMI160_FLAT_ENABLE
 *
 *	@param v_intr_enable_zero_u8 : The interrupt enable value
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 */
static BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_enable_0(
u8 v_enable_u8, u8 v_intr_enable_zero_u8)
{
	struct step_c_i2c_data *obj = obj_i2c_data;
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	if (obj == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		switch (v_enable_u8) {
		case BMI160_ANY_MOTION_X_ENABLE:
			/* write any motion x*/
			com_rslt = stc_i2c_read_block(obj_i2c_data->client,
					BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__REG,
					&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE,
				v_intr_enable_zero_u8);
				com_rslt += stc_i2c_write_block(obj_i2c_data->client,
					BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__REG,
					&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_ANY_MOTION_Y_ENABLE:
			/* write any motion y*/
			com_rslt = stc_i2c_read_block(obj_i2c_data->client,
					BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE,
				v_intr_enable_zero_u8);
				com_rslt += stc_i2c_write_block(obj_i2c_data->client,
				BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_ANY_MOTION_Z_ENABLE:
			/* write any motion z*/
			com_rslt = stc_i2c_read_block(obj_i2c_data->client,
					BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE,
				v_intr_enable_zero_u8);
				com_rslt += stc_i2c_write_block(obj_i2c_data->client,
				BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_DOUBLE_TAP_ENABLE:
			/* write double tap*/
			com_rslt = stc_i2c_read_block(obj_i2c_data->client,
				BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE,
				v_intr_enable_zero_u8);
				com_rslt += stc_i2c_write_block(obj_i2c_data->client,
				BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_SINGLE_TAP_ENABLE:
			/* write single tap */
			com_rslt = stc_i2c_read_block(obj_i2c_data->client,
					BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__REG,
					&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE,
				v_intr_enable_zero_u8);
				com_rslt += stc_i2c_write_block(obj_i2c_data->client,
				BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_ORIENT_ENABLE:
			/* write orient interrupt*/
			com_rslt = stc_i2c_read_block(obj_i2c_data->client,
					BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__REG,
					&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE,
				v_intr_enable_zero_u8);
				com_rslt += stc_i2c_write_block(obj_i2c_data->client,
					BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__REG,
					&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_FLAT_ENABLE:
			/* write flat interrupt*/
			com_rslt = stc_i2c_read_block(obj_i2c_data->client,
					BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__REG,
					&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE,
				v_intr_enable_zero_u8);
				com_rslt += stc_i2c_write_block(obj_i2c_data->client,
					BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__REG,
					&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}

static int bmi160_acc_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres = 0;
	u8 v_data_u8r = C_BMI160_ZERO_U8X;
	comres = stc_i2c_read_block(client,
			BMI160_USER_ACC_PMU_STATUS__REG, &v_data_u8r, 1);
	*mode = BMI160_GET_BITSLICE(v_data_u8r,
			BMI160_USER_ACC_PMU_STATUS);
	return comres;
}

static int signification_motion_enable(int enable)
{
	int err;
	u8 acc_op_mode = 0;
	struct step_c_i2c_data *obj = obj_i2c_data;
	err = bmi160_acc_get_mode(obj->client, &acc_op_mode);
	if (err < 0) {
		STEP_C_ERR("get acc mode failed.\n");
		return -EIO;
	}
	/*0x62 (bit 1) INT_MOTION_3 int_sig_mot_sel*/
	err = bmi160_set_intr_significant_motion_select((u8)enable);
	if (err < 0) {
		STEP_C_ERR("set significant motion failed.\n");
		return -EIO;
	}
	if ((BMI160_ACC_MODE_NORMAL == acc_op_mode) && (enable == 1)) {
		err = bmi160_set_intr_enable_0(BMI160_ANY_MOTION_X_ENABLE, 1);
		err += bmi160_set_intr_enable_0(BMI160_ANY_MOTION_Y_ENABLE, 1);
		err += bmi160_set_intr_enable_0(BMI160_ANY_MOTION_Z_ENABLE, 1);
		if (err < 0) {
			STEP_C_ERR("set intr enable failed.\n");
			return -EIO;
		}
	}else {
		err = bmi160_set_intr_enable_0(BMI160_ANY_MOTION_X_ENABLE, 0);
		err += bmi160_set_intr_enable_0(BMI160_ANY_MOTION_Y_ENABLE, 0);
		err += bmi160_set_intr_enable_0(BMI160_ANY_MOTION_Z_ENABLE, 0);
		if (err < 0)
			STEP_C_ERR("set intr disable failed.\n");
			return -EIO;
	}
	return err;
}

static int step_c_enable_significant(int open)
{
	int err = 0;
	err = signification_motion_enable(open);
	if(err < 0) {
		STEP_C_ERR("set significant motion failed.\n");
	}
	return err;
}

static int step_c_enable_step_detect(int open)
{
	return 0;
}

static int step_c_get_data(u64 *value, int *status)
{
	return 0;
}

static int stc_get_data_significant(u64 *value, int *status)
{
	return 0;
}

static int stc_get_data_step_d(u64 *value, int *status)
{
	return 0;
}

static int step_c_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct step_c_i2c_data *obj;
	struct step_c_control_path ctl = {0};
	struct step_c_data_path data = {0};
	int err = 0;
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	printk("lct1, %s, %d \n", __func__, __LINE__);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}
	printk("lct1, %s, %d \n", __func__, __LINE__);
	obj_i2c_data = obj;
	obj->client = bmi160_acc_i2c_client;
	i2c_set_clientdata(obj->client, obj);
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	obj->power_mode = STC_UNDEFINED_POWERMODE;
	obj->datarate = BMI160_ACCEL_ODR_RESERVED;
	mutex_init(&obj->lock);
	err = step_c_init_client(obj->client);
	if (err)
		goto exit_init_client_failed;

	err = misc_register(&step_c_device);
	if (err) {
		STEP_C_ERR("misc device register failed, err = %d\n", err);
		goto exit_misc_device_register_failed;
	}
	ctl.open_report_data= step_c_open_report_data;
	ctl.enable_nodata = step_c_enable_nodata;
	ctl.set_delay = step_c_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = false;
	ctl.enable_significant = step_c_enable_significant;
	ctl.enable_step_detect = step_c_enable_step_detect;
	err = step_c_register_control_path(&ctl);
	if(err) {
		STEP_C_ERR("register step_counter control path err.\n");
		goto exit_create_attr_failed;
	}
	data.get_data = step_c_get_data;
	data.vender_div = 1000;
	data.get_data_significant = stc_get_data_significant;
	data.get_data_step_d = stc_get_data_step_d;
	err = step_c_register_data_path(&data);
	if(err) {
		STEP_C_ERR("step_c_register_data_path fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	step_c_init_flag = 0;
	STEP_C_LOG("%s: is ok.\n", __func__);
	printk("lct1, %s, %d \n", __func__, __LINE__);
	return 0;

exit_create_attr_failed:
	misc_deregister(&step_c_device);
exit_misc_device_register_failed:
exit_init_client_failed:
	kfree(obj);
exit:
	step_c_init_flag =-1;
	STEP_C_ERR("err = %d\n", err);
	return err;
}

static int step_c_i2c_remove(struct i2c_client *client)
{
	int err = 0;
	err = misc_deregister(&step_c_device);
	if (err)
		STEP_C_ERR("misc_deregister failed, err = %d\n", err);
	obj_i2c_data = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}


int bmi160_step_notify(STEP_NOTIFY_TYPE type)
{
	return step_notify(type);
}

#ifdef CONFIG_OF
static const struct of_device_id step_c_of_match[] = {
	{.compatible = "mediatek,step_counter"},
	{},
};
#endif

static struct i2c_driver step_c_i2c_driver = {
	.driver = {
		.name = STC_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = step_c_of_match,
#endif
	},
	.probe = step_c_i2c_probe,
	.remove	= step_c_i2c_remove,
	.detect	= step_c_i2c_detect,
	.suspend = step_c_suspend,
	.resume = step_c_resume,
	.id_table = step_c_i2c_id,
};

static int bmi160_stc_remove(void)
{
    i2c_del_driver(&step_c_i2c_driver);
    return 0;
}

static int bmi160_stc_local_init(struct platform_device *pdev)
{
	printk("lct1, %s, %d \n", __func__, __LINE__);
	if(i2c_add_driver(&step_c_i2c_driver)) {
	printk("lct1, %s, %d \n", __func__, __LINE__);
		STEP_C_ERR("add driver error.\n");
		return -1;
	}
	if(-1 == step_c_init_flag) {
	   return -1;
	}
	return 0;
}

static struct step_c_init_info bmi160_stc_init_info = {
	.name = "step_counter",
	.init = bmi160_stc_local_init,
	.uninit = bmi160_stc_remove,
};

static struct i2c_board_info __initdata bmi160_acc_i2c_info ={ I2C_BOARD_INFO("step_counter", 0x7A)};
static int __init stc_init(void)
{
	//hw = get_accel_dts_func(COMPATIABLE_NAME, hw);
	hw = get_cust_acc_hw();
	printk("lct1, %s, %d \n", __func__, __LINE__);

	i2c_register_board_info(hw->i2c_num, &bmi160_acc_i2c_info, 1);
	if (!hw) {
		STEP_C_ERR("get dts info fail\n");
	printk("lct1, %s, %d \n", __func__, __LINE__);
		return 0;
	}
	step_c_driver_add(&bmi160_stc_init_info);
	return 0;
}

static void __exit stc_exit(void)
{
	STEP_C_FUN();
}

module_init(stc_init);
module_exit(stc_exit);

MODULE_LICENSE("GPLv2");
MODULE_DESCRIPTION("STEP COUNTER I2C Driver");
MODULE_AUTHOR("bosch@cn.bosch.com");
MODULE_VERSION(STC_DRIVER_VERSION);
