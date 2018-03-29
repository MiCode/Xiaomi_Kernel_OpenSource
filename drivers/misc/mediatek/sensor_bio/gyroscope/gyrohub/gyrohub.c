/* GYRO_HUB motion sensor driver
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
#include <hwmsensor.h>
#include "gyrohub.h"
#include <gyroscope.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define GYROHUB_DEV_NAME        "gyro_hub"	/* name must different with gsensor gyrohub */

#define GYROS_TAG					"[GYRO] "
#define GYROS_FUN(f)				pr_debug(GYROS_TAG"%s\n", __func__)
#define GYROS_ERR(fmt, args...)		pr_err(GYROS_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GYROS_LOG(fmt, args...)		pr_debug(GYROS_TAG fmt, ##args)


static struct gyro_init_info gyrohub_init_info;
struct platform_device *gyroPltFmDev;
static int gyrohub_init_flag = -1;

typedef enum {
	GYRO_TRC_FILTER = 0x01,
	GYRO_TRC_RAWDATA = 0x02,
	GYRO_TRC_IOCTL = 0x04,
	GYRO_TRC_CALI = 0X08,
	GYRO_TRC_INFO = 0X10,
	GYRO_TRC_DATA = 0X20,
} GYRO_TRC;
struct gyrohub_ipi_data {
	int direction;
	atomic_t trace;
	atomic_t suspend;
	int cali_sw[GYROHUB_AXES_NUM + 1];
	struct work_struct init_done_work;
	/*data */
	atomic_t scp_init_done;
	atomic_t first_ready_after_boot;
};
static struct gyrohub_ipi_data *obj_ipi_data;

static int gyrohub_write_rel_calibration(struct gyrohub_ipi_data *obj, int dat[GYROHUB_AXES_NUM])
{
	obj->cali_sw[GYROHUB_AXIS_X] = dat[GYROHUB_AXIS_X];
	obj->cali_sw[GYROHUB_AXIS_Y] = dat[GYROHUB_AXIS_Y];
	obj->cali_sw[GYROHUB_AXIS_Z] = dat[GYROHUB_AXIS_Z];


	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		GYROS_LOG("write gyro calibration data  (%5d, %5d, %5d)\n",
			 obj->cali_sw[GYROHUB_AXIS_X], obj->cali_sw[GYROHUB_AXIS_Y], obj->cali_sw[GYROHUB_AXIS_Z]);
	}

	return 0;
}

static int gyrohub_ResetCalibration(void)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	unsigned char buf[2];
	int err = 0;

	if (!atomic_read(&obj->scp_init_done)) {
		GYROS_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_RESET_CALI, buf);
	if (err < 0)
		GYROS_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
			ID_GYROSCOPE, CUST_ACTION_RESET_CALI);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return err;
}

static int gyrohub_ReadCalibration(int dat[GYROHUB_AXES_NUM])
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	dat[GYROHUB_AXIS_X] = obj->cali_sw[GYROHUB_AXIS_X];
	dat[GYROHUB_AXIS_Y] = obj->cali_sw[GYROHUB_AXIS_Y];
	dat[GYROHUB_AXIS_Z] = obj->cali_sw[GYROHUB_AXIS_Z];


	if (atomic_read(&obj->trace) & GYRO_TRC_CALI)
		GYROS_LOG("Read gyro calibration data  (%5d, %5d, %5d)\n",
			 dat[GYROHUB_AXIS_X], dat[GYROHUB_AXIS_Y], dat[GYROHUB_AXIS_Z]);


	return 0;
}

static int gyrohub_WriteCalibration_scp(int dat[GYROHUB_AXES_NUM])
{
	int err = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	if (!atomic_read(&obj->scp_init_done)) {
		GYROS_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_SET_CALI, dat);
	if (err < 0)
		GYROS_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
			ID_GYROSCOPE, CUST_ACTION_SET_CALI);
	return err;
}

static int gyrohub_WriteCalibration(int dat[GYROHUB_AXES_NUM])
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	int err = 0;
	int cali[GYROHUB_AXES_NUM];

	GYROS_FUN();

	if (!obj || !dat) {
		GYROS_ERR("null ptr!!\n");
		return -EINVAL;
	}

	err = gyrohub_WriteCalibration_scp(dat);
	if (err < 0) {
		GYROS_ERR("gyrohub_WriteCalibration_scp fail\n");
		return -1;
	}

	cali[GYROHUB_AXIS_X] = obj->cali_sw[GYROHUB_AXIS_X];
	cali[GYROHUB_AXIS_Y] = obj->cali_sw[GYROHUB_AXIS_Y];
	cali[GYROHUB_AXIS_Z] = obj->cali_sw[GYROHUB_AXIS_Z];

	cali[GYROHUB_AXIS_X] += dat[GYROHUB_AXIS_X];
	cali[GYROHUB_AXIS_Y] += dat[GYROHUB_AXIS_Y];
	cali[GYROHUB_AXIS_Z] += dat[GYROHUB_AXIS_Z];

	if (atomic_read(&obj->trace) & GYRO_TRC_CALI)
		GYROS_LOG("write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
			 dat[GYROHUB_AXIS_X], dat[GYROHUB_AXIS_Y], dat[GYROHUB_AXIS_Z],
			 cali[GYROHUB_AXIS_X], cali[GYROHUB_AXIS_Y], cali[GYROHUB_AXIS_Z]);

	return gyrohub_write_rel_calibration(obj, cali);
}


static int gyrohub_SetPowerMode(bool enable)
{
	int err = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	if (!atomic_read(&obj->scp_init_done)) {
		GYROS_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_enable_to_hub(ID_GYROSCOPE, enable);
	if (err < 0)
		GYROS_ERR("sensor_enable_to_hub fail!\n");

	return err;
}

static int gyrohub_ReadGyroData(char *buf, int bufsize)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;
	int gyro[GYROHUB_AXES_NUM];
	int err = 0;
	int status = 0;

	if (!atomic_read(&obj->scp_init_done)) {
		GYROS_ERR("sensor hub has not been ready!!\n");
		return -1;
	}

	if (atomic_read(&obj->suspend))
		return -3;

	if (NULL == buf)
		return -1;
	err = sensor_get_data_from_hub(ID_GYROSCOPE, &data);
	if (err < 0) {
		GYROS_ERR("sensor_get_data_from_hub fail!\n");
		return err;
	}

	time_stamp				= data.time_stamp;
	time_stamp_gpt			= data.time_stamp_gpt;
	gyro[GYROHUB_AXIS_X]	= data.gyroscope_t.x;
	gyro[GYROHUB_AXIS_Y]	= data.gyroscope_t.y;
	gyro[GYROHUB_AXIS_Z]	= data.gyroscope_t.z;
	status					= data.gyroscope_t.status;
	GYROS_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, x: %d, y: %d, z: %d!\n", time_stamp, time_stamp_gpt,
		gyro[GYROHUB_AXIS_X], gyro[GYROHUB_AXIS_Y], gyro[GYROHUB_AXIS_Z]);


	sprintf(buf, "%04x %04x %04x %04x", gyro[GYROHUB_AXIS_X], gyro[GYROHUB_AXIS_Y], gyro[GYROHUB_AXIS_Z], status);

	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYROS_LOG("gsensor data: %s!\n", buf);

	return 0;

}

static int gyrohub_ReadChipInfo(char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8) * 10);

	if ((NULL == buf) || (bufsize <= 30))
		return -1;

	sprintf(buf, "GYROHUB Chip");
	return 0;
}

static int gyrohub_ReadAllReg(char *buf, int bufsize)
{
	int err = 0;

	err = gyrohub_SetPowerMode(true);
	if (err)
		GYROS_ERR("Power on mpu6050 error %d!\n", err);
	msleep(50);
	err = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_SHOW_REG, buf);
	if (err < 0) {
		GYROS_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n", ID_GYROSCOPE, CUST_ACTION_SHOW_REG);
		return 0;
	}
	return 0;
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	char strbuf[GYROHUB_BUFSIZE];
	int err = 0;

	if (NULL == obj) {
		GYROS_ERR("obj is null!!\n");
		return 0;
	}
	err = gyrohub_ReadAllReg(strbuf, GYROHUB_BUFSIZE);
	if (err < 0) {
		GYROS_LOG("gyrohub_ReadAllReg fail!!\n");
		return 0;
	}
	err = gyrohub_ReadChipInfo(strbuf, GYROHUB_BUFSIZE);
	if (err < 0) {
		GYROS_LOG("gyrohub_ReadChipInfo fail!!\n");
		return 0;
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	char strbuf[GYROHUB_BUFSIZE];
	int err = 0;

	if (NULL == obj) {
		GYROS_ERR("obj is null!!\n");
		return 0;
	}

	err = gyrohub_ReadGyroData(strbuf, GYROHUB_BUFSIZE);
	if (err < 0) {
		GYROS_LOG("gyrohub_ReadGyroData fail!!\n");
		return 0;
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	if (obj == NULL) {
		GYROS_ERR(" obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	int trace = 0;
	int res = 0;

	if (obj == NULL) {
		GYROS_ERR("obj is null!!\n");
		return 0;
	}
	if (!atomic_read(&obj->scp_init_done)) {
		GYROS_ERR("sensor hub has not been ready!!\n");
		return 0;
	}
	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
		res = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_SET_TRACE, &trace);
		if (res < 0) {
			GYROS_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
				ID_GYROSCOPE, CUST_ACTION_SET_TRACE);
			return 0;
		}
	} else {
		GYROS_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}
	return count;
}

static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	if (obj == NULL) {
		GYROS_ERR(" obj is null!!\n");
		return 0;
	}

	return len;
}

static ssize_t show_chip_orientation(struct device_driver *ddri, char *buf)
{
	ssize_t _tLength = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	_tLength = snprintf(buf, PAGE_SIZE, "default direction = %d\n", obj->direction);

	return _tLength;
}

static ssize_t store_chip_orientation(struct device_driver *ddri, const char *buf, size_t tCount)
{
	int _nDirection = 0, ret = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	if (NULL == obj)
		return 0;
	if (!atomic_read(&obj->scp_init_done)) {
		GYROS_ERR("sensor hub has not been ready!!\n");
		return 0;
	}
	ret = kstrtoint(buf, 10, &_nDirection);
	if (ret == 0) {
		obj->direction = _nDirection;
		ret = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_SET_DIRECTION, &_nDirection);
		if (ret < 0) {
			GYROS_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
				ID_GYROSCOPE, CUST_ACTION_SET_DIRECTION);
			return 0;
		}
	}

	GYROS_LOG("[%s] set direction: %d\n", __func__, _nDirection);

	return tCount;
}

static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(orientation, S_IWUSR | S_IRUGO, show_chip_orientation, store_chip_orientation);

static struct driver_attribute *gyrohub_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_trace,	/*trace log */
	&driver_attr_status,
	&driver_attr_orientation,
};

static int gyrohub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(gyrohub_attr_list) / sizeof(gyrohub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, gyrohub_attr_list[idx]);
		if (0 != err) {
			GYROS_ERR("driver_create_file (%s) = %d\n", gyrohub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int gyrohub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(gyrohub_attr_list) / sizeof(gyrohub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, gyrohub_attr_list[idx]);

	return err;
}

static void scp_init_work_done(struct work_struct *work)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	int err = 0;

	if (atomic_read(&obj->scp_init_done) == 0) {
		GYROS_ERR("scp is not ready to send cmd\n");
	} else {
		if (0 == atomic_read(&obj->first_ready_after_boot)) {
			atomic_set(&obj->first_ready_after_boot, 1);
		} else {
			err = gyrohub_WriteCalibration_scp(obj->cali_sw);
			if (err < 0)
				GYROS_ERR("gyrohub_WriteCalibration_scp fail\n");
		}
	}
}

static int gyro_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		err = gyro_flush_report();
	else if (event->flush_action == DATA_ACTION)
		err = gyro_data_report(event->gyroscope_t.x, event->gyroscope_t.y, event->gyroscope_t.z,
			event->gyroscope_t.status, (int64_t)(event->time_stamp + event->time_stamp_gpt));
	else if (event->flush_action == BIAS_ACTION)
		err = gyro_bias_report(event->gyroscope_t.x_bias, event->gyroscope_t.y_bias, event->gyroscope_t.z_bias);
	return err;
}

static int gyrohub_open(struct inode *inode, struct file *file)
{
	file->private_data = obj_ipi_data;

	if (file->private_data == NULL) {
		GYROS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int gyrohub_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
static long gyrohub_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	char strbuf[GYROHUB_BUFSIZE] = { 0 };
	void __user *data;
	long err = 0;
	int use_in_factory_mode = USE_IN_FACTORY_MODE;
	struct SENSOR_DATA sensor_data;
	int cali[3];
	int smtRes = 0;
	int copy_cnt = 0;
	static int first_time_enable;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		GYROS_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GYROSCOPE_IOCTL_INIT:

		break;
	case GYROSCOPE_IOCTL_SMT_DATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		copy_cnt = copy_to_user(data, &smtRes, sizeof(smtRes));
		if (copy_cnt) {
			err = -EFAULT;
			GYROS_ERR("copy gyro data to user failed!\n");
		}
		err = 0;
		break;
	case GYROSCOPE_IOCTL_READ_SENSORDATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (first_time_enable == 0) {
			err = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_SET_FACTORY, &use_in_factory_mode);
			if (err < 0) {
				GYROS_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
					ID_GYROSCOPE, CUST_ACTION_SET_TRACE);
				return 0;
			}
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
			err = sensor_enable_to_hub(ID_GYROSCOPE, 1);
			if (err) {
				GYROS_ERR("gyrohub_ReadGyroData failed!\n");
				break;
			}
			err = sensor_set_delay_to_hub(ID_GYROSCOPE, 25);
			if (err) {
				GYROS_ERR("sensor_set_delay_to_hub failed!\n");
				break;
			}
#elif defined CONFIG_NANOHUB
			err = sensor_set_delay_to_hub(ID_GYROSCOPE, 20);
			if (err) {
				GYROS_ERR("sensor_set_delay_to_hub failed!\n");
				break;
			}
			err = sensor_enable_to_hub(ID_GYROSCOPE, 1);
			if (err) {
				GYROS_ERR("gyrohub_ReadGyroData failed!\n");
				break;
			}
#else

#endif
			first_time_enable = 1;
		}
		err = gyrohub_ReadGyroData(strbuf, GYROHUB_BUFSIZE);
		if (err) {
			GYROS_ERR("gyrohub_ReadGyroData failed!\n");
			break;
		}
		if (copy_to_user(data, strbuf, sizeof(strbuf))) {
			err = -EFAULT;
			break;
		}
		break;

	case GYROSCOPE_IOCTL_SET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}

		else {
			cali[GYROHUB_AXIS_X] = sensor_data.x;
			cali[GYROHUB_AXIS_Y] = sensor_data.y;
			cali[GYROHUB_AXIS_Z] = sensor_data.z;
			GYROS_LOG("gyro set cali:[%5d %5d %5d]\n",
				 cali[GYROHUB_AXIS_X], cali[GYROHUB_AXIS_Y], cali[GYROHUB_AXIS_Z]);
			err = gyrohub_WriteCalibration(cali);
			if (err) {
				GYROS_ERR("gyrohub_WriteCalibration failed!\n");
				break;
			}
		}
		break;

	case GYROSCOPE_IOCTL_CLR_CALI:
		err = gyrohub_ResetCalibration();
		if (err) {
			GYROS_ERR("gyrohub_ResetCalibration failed!\n");
			break;
		}
		break;

	case GYROSCOPE_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = gyrohub_ReadCalibration(cali);
		if (err)
			break;

		sensor_data.x = cali[GYROHUB_AXIS_X];
		sensor_data.y = cali[GYROHUB_AXIS_Y];
		sensor_data.z = cali[GYROHUB_AXIS_Z];
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		GYROS_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}
	return err;
}

#ifdef CONFIG_COMPAT
static long gyrohub_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;


	switch (cmd) {
	case COMPAT_GYROSCOPE_IOCTL_INIT:
		if (arg32 == NULL) {
			GYROS_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_INIT, (unsigned long)arg32);
		if (ret) {
			GYROS_ERR("GYROSCOPE_IOCTL_INIT unlocked_ioctl failed.\n");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_SET_CALI:
		if (arg32 == NULL) {
			GYROS_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_SET_CALI, (unsigned long)arg32);
		if (ret) {
			GYROS_ERR("GYROSCOPE_IOCTL_SET_CALI unlocked_ioctl failed.\n");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_CLR_CALI:
		if (arg32 == NULL) {
			GYROS_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_CLR_CALI, (unsigned long)arg32);
		if (ret) {
			GYROS_ERR("GYROSCOPE_IOCTL_CLR_CALI unlocked_ioctl failed.\n");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_GET_CALI:
		if (arg32 == NULL) {
			GYROS_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_GET_CALI, (unsigned long)arg32);
		if (ret) {
			GYROS_ERR("GYROSCOPE_IOCTL_GET_CALI unlocked_ioctl failed.\n");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA:
		if (arg32 == NULL) {
			GYROS_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_READ_SENSORDATA, (unsigned long)arg32);
		if (ret) {
			GYROS_ERR("GYROSCOPE_IOCTL_READ_SENSORDATA unlocked_ioctl failed.\n");
			return ret;
		}

		break;

	default:
		GYROS_ERR("%s not supported = 0x%04x\n", __func__, cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif
static const struct file_operations gyrohub_fops = {
	.open = gyrohub_open,
	.release = gyrohub_release,
	.unlocked_ioctl = gyrohub_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gyrohub_compat_ioctl,
#endif
};

static struct miscdevice gyrohub_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyroscope",
	.fops = &gyrohub_fops,
};
static int gyrohub_open_report_data(int open)
{
	return 0;
}


static int gyrohub_enable_nodata(int en)
{
	int res = 0;
	bool power = false;

	if (1 == en)
		power = true;
	if (0 == en)
		power = false;

	res = gyrohub_SetPowerMode(power);
	if (res < 0) {
		GYROS_ERR("GYROHUB_SetPowerMode fail\n");
		return res;
	}
	GYROS_LOG("gyrohub_enable_nodata OK!\n");
	return 0;

}

static int gyrohub_set_delay(u64 ns)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	int err = 0;
	int value = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	value = (int)ns / 1000 / 1000;
	if (!atomic_read(&obj->scp_init_done)) {
		GYROS_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_set_delay_to_hub(ID_GYROSCOPE, value);
	if (err < 0) {
		GYROS_ERR("sensor_set_delay_to_hub fail!\n");
		return err;
	}

	GYROS_LOG("gyro_set_delay (%d)\n", value);
	return err;
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int gyrohub_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_GYROSCOPE, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int gyrohub_flush(void)
{
	return sensor_flush_to_hub(ID_GYROSCOPE);
}

static int gyrohub_set_cali(uint8_t *data, uint8_t count)
{
	return sensor_cfg_to_hub(ID_GYROSCOPE, data, count);
}

static int gpio_config(void)
{
	int ret;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;

	pinctrl = devm_pinctrl_get(&gyroPltFmDev->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		GYRO_ERR("Cannot find gyro pinctrl!\n");
		return ret;
	}
	pins_default = pinctrl_lookup_state(pinctrl, "pin_default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		GYRO_ERR("Cannot find gyro pinctrl default!\n");
	}

	pins_cfg = pinctrl_lookup_state(pinctrl, "pin_cfg");
	if (IS_ERR(pins_cfg)) {
		ret = PTR_ERR(pins_cfg);
		GYRO_ERR("Cannot find gyro pinctrl pin_cfg!\n");
		return ret;
	}
	pinctrl_select_state(pinctrl, pins_cfg);

	return 0;
}

static int gyrohub_get_data(int *x, int *y, int *z, int *status)
{
	char buff[GYROHUB_BUFSIZE];
	int err = 0;

	err = gyrohub_ReadGyroData(buff, GYROHUB_BUFSIZE);
	if (err < 0) {
		GYROS_ERR("gyrohub_ReadGyroData fail!!\n");
		return -1;
	}
	err = sscanf(buff, "%x %x %x %x", x, y, z, status);
	if (err != 4) {
		GYROS_ERR("sscanf fail!!\n");
		return -1;
	}
	return 0;
}
static int scp_ready_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	switch (event) {
	case SCP_EVENT_READY:
	    atomic_set(&obj->scp_init_done, 1);
			schedule_work(&obj->init_done_work);
	    break;
	case SCP_EVENT_STOP:
	    atomic_set(&obj->scp_init_done, 0);
	    break;
	}
	return NOTIFY_DONE;
}
static struct notifier_block scp_ready_notifier = {
	.notifier_call = scp_ready_event,
};
static int gyrohub_probe(struct platform_device *pdev)
{
	struct gyrohub_ipi_data *obj;
	int err = 0;
	struct gyro_control_path ctl = { 0 };
	struct gyro_data_path data = { 0 };

	GYROS_FUN();
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct gyrohub_ipi_data));

	obj_ipi_data = obj;
	platform_set_drvdata(pdev, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	atomic_set(&obj->first_ready_after_boot, 0);
	atomic_set(&obj->scp_init_done, 0);
	INIT_WORK(&obj->init_done_work, scp_init_work_done);

	err = gpio_config();
	if (err < 0) {
		GYROS_ERR("gpio_config failed\n");
		goto exit_kfree;
	}
	scp_register_notify(&scp_ready_notifier);
	err = SCP_sensorHub_data_registration(ID_GYROSCOPE, gyro_recv_data);
	if (err < 0) {
		GYROS_ERR("SCP_sensorHub_data_registration failed\n");
		goto exit_kfree;
	}
	err = misc_register(&gyrohub_misc_device);
	if (err) {
		GYROS_ERR("gyrohub_misc_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}
	ctl.is_use_common_factory = false;

	err = gyrohub_create_attr(&(gyrohub_init_info.platform_diver_addr->driver));
	if (err) {
		GYROS_ERR("gyrohub create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = gyrohub_open_report_data;
	ctl.enable_nodata = gyrohub_enable_nodata;
	ctl.set_delay = gyrohub_set_delay;
	ctl.batch = gyrohub_batch;
	ctl.flush = gyrohub_flush;
	ctl.set_cali = gyrohub_set_cali;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#else
#endif

	err = gyro_register_control_path(&ctl);
	if (err) {
		GYROS_ERR("register gyro control path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = gyrohub_get_data;
	data.vender_div = DEGREE_TO_RAD;
	err = gyro_register_data_path(&data);
	if (err) {
		GYROS_ERR("gyro_register_data_path fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	gyrohub_init_flag = 0;

	GYROS_LOG("%s: OK\n", __func__);
	return 0;
exit_create_attr_failed:
	gyrohub_delete_attr(&(gyrohub_init_info.platform_diver_addr->driver));
exit_misc_device_register_failed:
	misc_deregister(&gyrohub_misc_device);
exit_kfree:
	kfree(obj);
exit:
	gyrohub_init_flag = -1;
	GYROS_ERR("%s: err = %d\n", __func__, err);
	return err;
}

static int gyrohub_remove(struct platform_device *pdev)
{
	int err = 0;

	err = gyrohub_delete_attr(&(gyrohub_init_info.platform_diver_addr->driver));
	if (err)
		GYROS_ERR("gyrohub_delete_attr fail: %d\n", err);

	err = misc_deregister(&gyrohub_misc_device);
	if (err)
		GYROS_ERR("misc_deregister fail: %d\n", err);

	kfree(platform_get_drvdata(pdev));
	return 0;
}

static int gyrohub_suspend(struct platform_device *pdev, pm_message_t msg)
{
	return 0;
}

static int gyrohub_resume(struct platform_device *pdev)
{
	return 0;
}
static struct platform_device gyrohub_device = {
	.name = GYROHUB_DEV_NAME,
	.id = -1,
};
static struct platform_driver gyrohub_driver = {
	.driver = {
		   .name = GYROHUB_DEV_NAME,
	},
	.probe = gyrohub_probe,
	.remove = gyrohub_remove,
	.suspend = gyrohub_suspend,
	.resume = gyrohub_resume,
};

static int gyrohub_local_remove(void)
{
	platform_driver_unregister(&gyrohub_driver);
	return 0;
}

static int gyrohub_local_init(struct platform_device *pdev)
{
	gyroPltFmDev = pdev;

	if (platform_driver_register(&gyrohub_driver)) {
		GYROS_ERR("add driver error\n");
		return -1;
	}
	if (-1 == gyrohub_init_flag)
		return -1;
	return 0;
}
static struct gyro_init_info gyrohub_init_info = {
	.name = "gyrohub",
	.init = gyrohub_local_init,
	.uninit = gyrohub_local_remove,
};

static int __init gyrohub_init(void)
{

	if (platform_device_register(&gyrohub_device)) {
		GYROS_ERR("platform device error\n");
		return -1;
	}
	gyro_driver_add(&gyrohub_init_info);

	return 0;
}

static void __exit gyrohub_exit(void)
{
	GYROS_FUN();
}

module_init(gyrohub_init);
module_exit(gyrohub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GYROHUB gyroscope driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
