/* accelhub motion sensor driver
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
#include "accelhub.h"
#include <accel.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define DEBUG 1
#define SW_CALIBRATION
#define ACCELHUB_AXIS_X          0
#define ACCELHUB_AXIS_Y          1
#define ACCELHUB_AXIS_Z          2
#define ACCELHUB_AXES_NUM        3
#define ACCELHUB_DATA_LEN        6
#define ACCELHUB_DEV_NAME        "accel_hub_pl"	/* name must different with gyro accelhub */
/* dadadadada */
typedef enum {
	ACCELHUB_TRC_FILTER = 0x01,
	ACCELHUB_TRC_RAWDATA = 0x02,
	ACCELHUB_TRC_IOCTL = 0x04,
	ACCELHUB_TRC_CALI = 0X08,
	ACCELHUB_TRC_INFO = 0X10,
} ACCELHUB_TRC;
struct accelhub_ipi_data {
	/*misc */
	atomic_t trace;
	atomic_t suspend;
	int cali_sw[ACCELHUB_AXES_NUM + 1];
	int direction;
	struct work_struct init_done_work;
	atomic_t scp_init_done;
	atomic_t first_ready_after_boot;
};

static struct acc_init_info accelhub_init_info;

static struct accelhub_ipi_data *obj_ipi_data;

static int gsensor_init_flag = -1;

#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               pr_debug(GSE_TAG"%s\n", __func__)
#define GSE_ERR(fmt, args...)    pr_err(GSE_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    pr_debug(GSE_TAG fmt, ##args)

int accelhub_SetPowerMode(bool enable)
{
	int err = 0;
	struct accelhub_ipi_data *obj = obj_ipi_data;

	if (!atomic_read(&obj->scp_init_done)) {
		GSE_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_enable_to_hub(ID_ACCELEROMETER, enable);
	if (err < 0) {
		GSE_ERR("SCP_sensorHub_req_send fail!\n");
		return err;
	}
	return err;
}

static int accelhub_ReadCalibration(int dat[ACCELHUB_AXES_NUM])
{
	struct accelhub_ipi_data *obj = obj_ipi_data;

	dat[ACCELHUB_AXIS_X] = obj->cali_sw[ACCELHUB_AXIS_X];
	dat[ACCELHUB_AXIS_Y] = obj->cali_sw[ACCELHUB_AXIS_Y];
	dat[ACCELHUB_AXIS_Z] = obj->cali_sw[ACCELHUB_AXIS_Z];

	return 0;
}

static int accelhub_ResetCalibration(void)
{
	struct accelhub_ipi_data *obj = obj_ipi_data;
	int err = 0;
	unsigned char dat[2];

	if (!atomic_read(&obj->scp_init_done)) {
		GSE_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_set_cmd_to_hub(ID_ACCELEROMETER, CUST_ACTION_RESET_CALI, dat);
	if (err < 0) {
		GSE_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n", ID_ACCELEROMETER,
			CUST_ACTION_RESET_CALI);
	}

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));

	return err;
}

static int accelhub_ReadCalibrationEx(int act[ACCELHUB_AXES_NUM], int raw[ACCELHUB_AXES_NUM])
{
	/*raw: the raw calibration data; act: the actual calibration data */
	struct accelhub_ipi_data *obj = obj_ipi_data;

	raw[ACCELHUB_AXIS_X] = obj->cali_sw[ACCELHUB_AXIS_X];
	raw[ACCELHUB_AXIS_Y] = obj->cali_sw[ACCELHUB_AXIS_Y];
	raw[ACCELHUB_AXIS_Z] = obj->cali_sw[ACCELHUB_AXIS_Z];

	act[ACCELHUB_AXIS_X] = raw[ACCELHUB_AXIS_X];
	act[ACCELHUB_AXIS_Y] = raw[ACCELHUB_AXIS_Y];
	act[ACCELHUB_AXIS_Z] = raw[ACCELHUB_AXIS_Z];

	return 0;
}

static int accelhub_WriteCalibration_scp(int dat[ACCELHUB_AXES_NUM])
{
	int err = 0;
	struct accelhub_ipi_data *obj = obj_ipi_data;

	if (!atomic_read(&obj->scp_init_done)) {
		GSE_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_set_cmd_to_hub(ID_ACCELEROMETER, CUST_ACTION_SET_CALI, dat);
	if (err < 0)
		GSE_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n", ID_ACCELEROMETER, CUST_ACTION_SET_CALI);
	return err;
}

static int accelhub_WriteCalibration(int dat[ACCELHUB_AXES_NUM])
{
	struct accelhub_ipi_data *obj = obj_ipi_data;
	int err = 0;
	int cali[ACCELHUB_AXES_NUM], raw[ACCELHUB_AXES_NUM];

	err = accelhub_ReadCalibrationEx(cali, raw);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d), cali: (%+3d %+3d %+3d)\n",
		raw[ACCELHUB_AXIS_X], raw[ACCELHUB_AXIS_Y], raw[ACCELHUB_AXIS_Z],
		obj->cali_sw[ACCELHUB_AXIS_X], obj->cali_sw[ACCELHUB_AXIS_Y], obj->cali_sw[ACCELHUB_AXIS_Z]);

	err = accelhub_WriteCalibration_scp(dat);
	if (err < 0) {
		GSE_ERR("accelhub_WriteCalibration_scp fail\n");
		return err;
	}
	/*calculate the real offset expected by caller */
	cali[ACCELHUB_AXIS_X] += dat[ACCELHUB_AXIS_X];
	cali[ACCELHUB_AXIS_Y] += dat[ACCELHUB_AXIS_Y];
	cali[ACCELHUB_AXIS_Z] += dat[ACCELHUB_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n", dat[ACCELHUB_AXIS_X], dat[ACCELHUB_AXIS_Y], dat[ACCELHUB_AXIS_Z]);

	obj->cali_sw[ACCELHUB_AXIS_X] = cali[ACCELHUB_AXIS_X];
	obj->cali_sw[ACCELHUB_AXIS_Y] = cali[ACCELHUB_AXIS_Y];
	obj->cali_sw[ACCELHUB_AXIS_Z] = cali[ACCELHUB_AXIS_Z];

	return err;
}

static int accelhub_ReadAllReg(char *buf, int bufsize)
{
	int err = 0;

	err = accelhub_SetPowerMode(true);
	if (err) {
		GSE_ERR("Power on accelhub error %d!\n", err);
		return err;
	}

	/* register map */
	return 0;
}

static int accelhub_ReadChipInfo(char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8) * 10);

	if ((NULL == buf) || (bufsize <= 30))
		return -1;

	sprintf(buf, "ACCELHUB Chip");
	return 0;
}

static int accelhub_ReadSensorData(char *buf, int bufsize)
{
	struct accelhub_ipi_data *obj = obj_ipi_data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;
	struct data_unit_t data;
	int acc[ACCELHUB_AXES_NUM];
	int err = 0;
	int status = 0;
	if (!atomic_read(&obj->scp_init_done)) {
		GSE_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	if (atomic_read(&obj->suspend))
		return -3;

	if (NULL == buf)
		return -1;
	err = sensor_get_data_from_hub(ID_ACCELEROMETER, &data);
	if (err < 0) {
		GSE_ERR("sensor_get_data_from_hub fail!\n");
		return err;
	}
	time_stamp = data.time_stamp;
	time_stamp_gpt = data.time_stamp_gpt;
	acc[ACCELHUB_AXIS_X] = data.accelerometer_t.x;
	acc[ACCELHUB_AXIS_Y] = data.accelerometer_t.y;
	acc[ACCELHUB_AXIS_Z] = data.accelerometer_t.z;
	status				 = data.accelerometer_t.status;
	/*GSE_ERR("accelhub_ReadSensorData: timestamp: %lld, timestamp_gpt: %lld, x: %d, y: %d, z: %d, status:%d!\n", time_stamp, time_stamp_gpt,
		acc[ACCELHUB_AXIS_X], acc[ACCELHUB_AXIS_Y], acc[ACCELHUB_AXIS_Z], status);*/

	sprintf(buf, "%04x %04x %04x %04x", acc[ACCELHUB_AXIS_X], acc[ACCELHUB_AXIS_Y], acc[ACCELHUB_AXIS_Z], status);
	if (atomic_read(&obj->trace) & ACCELHUB_TRC_IOCTL)
		GSE_LOG("gsensor data: %s!\n", buf);

	return 0;
}

static int accelhub_ReadRawData(char *buf)
{
	struct accelhub_ipi_data *obj = obj_ipi_data;

	if (!buf)
		return -3;

	if (atomic_read(&obj->suspend))
		return -1;

	return 0;
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[ACCELHUB_BUFSIZE];

	accelhub_SetPowerMode(true);
	msleep(50);

	accelhub_ReadAllReg(strbuf, ACCELHUB_BUFSIZE);

	accelhub_ReadChipInfo(strbuf, ACCELHUB_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[ACCELHUB_BUFSIZE];

	accelhub_ReadSensorData(strbuf, ACCELHUB_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct accelhub_ipi_data *obj = obj_ipi_data;
	int len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
		     obj->cali_sw[ACCELHUB_AXIS_X], obj->cali_sw[ACCELHUB_AXIS_Y], obj->cali_sw[ACCELHUB_AXIS_Z]);

	return len;
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct accelhub_ipi_data *obj = obj_ipi_data;
	int trace = 0;
	int res = 0;

	if (obj == NULL) {
		GSE_ERR("obj is null!!\n");
		return 0;
	}
	if (!atomic_read(&obj->scp_init_done)) {
		GSE_ERR("sensor hub has not been ready!!\n");
		return 0;
	}
	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
		res = sensor_set_cmd_to_hub(ID_ACCELEROMETER, CUST_ACTION_SET_TRACE, &trace);
		if (res < 0) {
			GSE_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n", ID_ACCELEROMETER,
				CUST_ACTION_SET_TRACE);
			return 0;
		}
	} else {
		GSE_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}

	return count;
}

static ssize_t show_chip_orientation(struct device_driver *ddri, char *buf)
{
	ssize_t _tLength = 0;
	struct accelhub_ipi_data *obj = obj_ipi_data;

	_tLength = snprintf(buf, PAGE_SIZE, "default direction = %d\n", obj->direction);

	return _tLength;
}

static ssize_t store_chip_orientation(struct device_driver *ddri, const char *buf, size_t tCount)
{
	int _nDirection = 0, ret = 0;
	struct accelhub_ipi_data *obj = obj_ipi_data;

	if (NULL == obj)
		return 0;
	if (!atomic_read(&obj->scp_init_done)) {
		GSE_ERR("sensor hub has not been ready!!\n");
		return 0;
	}
	ret = kstrtoint(buf, 10, &_nDirection);
	if (ret != 0) {
		GSE_LOG("kstrtoint fail\n");
		return 0;
	}
	obj->direction = _nDirection;
	ret = sensor_set_cmd_to_hub(ID_ACCELEROMETER, CUST_ACTION_SET_DIRECTION, &_nDirection);
	if (ret < 0) {
		GSE_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n", ID_ACCELEROMETER,
			CUST_ACTION_SET_DIRECTION);
		return 0;
	}

	GSE_LOG("[%s] set direction: %d\n", __func__, _nDirection);

	return tCount;
}

static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(cali, S_IWUSR | S_IRUGO, show_cali_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);
static DRIVER_ATTR(orientation, S_IWUSR | S_IRUGO, show_chip_orientation, store_chip_orientation);

static struct driver_attribute *accelhub_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_cali,	/*show calibration data */
	&driver_attr_trace,	/*trace log */
	&driver_attr_orientation,
};

static int accelhub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(accelhub_attr_list) / sizeof(accelhub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, accelhub_attr_list[idx]);
		if (0 != err) {
			GSE_ERR("driver_create_file (%s) = %d\n", accelhub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int accelhub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(accelhub_attr_list) / sizeof(accelhub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, accelhub_attr_list[idx]);

	return err;
}

static void scp_init_work_done(struct work_struct *work)
{
	struct accelhub_ipi_data *obj = obj_ipi_data;
	int err = 0;

	if (atomic_read(&obj->scp_init_done) == 0) {
		GSE_LOG("scp is not ready to send cmd\n");
	} else {
		if (0 == atomic_read(&obj->first_ready_after_boot)) {
			atomic_set(&obj->first_ready_after_boot, 1);
		} else {
			err = accelhub_WriteCalibration_scp(obj->cali_sw);
			if (err < 0)
				GSE_ERR("accelhub_WriteCalibration_scp fail\n");
		}
	}
}

static int gsensor_recv_interrput_ipidata(void *data, unsigned int len)
{
	struct accelhub_ipi_data *obj = obj_ipi_data;
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;

	GSE_FUN();
	GSE_ERR("len = %d, type = %d, action = %d, errCode = %d\n", len, rsp->rsp.sensorType, rsp->rsp.action,
		rsp->rsp.errCode);

	if (!obj)
		return -1;

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_NOTIFY:
			/* need add interrupt surpport later */
			break;
		default:
			GSE_ERR("Error sensor hub notify");
			break;
		}
		break;
	default:
		GSE_ERR("Error sensor hub action");
		break;
	}

	return 0;
}

static int accelhub_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int accelhub_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long accelhub_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct accelhub_ipi_data *obj = obj_ipi_data;
	char strbuf[ACCELHUB_BUFSIZE];
	void __user *data;
	int use_in_factory_mode = USE_IN_FACTORY_MODE;
	struct SENSOR_DATA sensor_data;
	long err = 0;
	int cali[3];
	static int first_time_enable = 0;
	
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
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

		err = accelhub_ReadChipInfo(strbuf, ACCELHUB_BUFSIZE);
		if (err < 0)
			break;
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
		if (first_time_enable == 0) {
			err = sensor_set_cmd_to_hub(ID_ACCELEROMETER, CUST_ACTION_SET_FACTORY, &use_in_factory_mode);
			if (err < 0) {
				GSE_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n", ID_ACCELEROMETER,
					CUST_ACTION_SET_TRACE);
				return 0;
			}		
			err = accelhub_SetPowerMode(true);
			if (err < 0) {
				GSE_ERR("accelhub_SetPowerMode fail\n");
				break;
			}
			err = sensor_set_delay_to_hub(ID_ACCELEROMETER, 20);
			if (err < 0) {
				GSE_ERR("sensor_set_delay_to_hub fail, (ID: %d),(action: %d)\n", ID_ACCELEROMETER,
					CUST_ACTION_SET_TRACE);
				return 0;
			}
			first_time_enable = 1;
		}
		err = accelhub_ReadSensorData(strbuf, ACCELHUB_BUFSIZE);
		if (err < 0) {
			GSE_ERR("accelhub_ReadSensorData fail\n");
			break;
		}
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

	case GSENSOR_IOCTL_READ_RAW_DATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (atomic_read(&obj->suspend)) {
			err = -EINVAL;
		} else {
			err = accelhub_ReadRawData(strbuf);
			if (err < 0) {
				GSE_ERR("accelhub_ReadRawData fail\n");
				break;
			}
			if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
				err = -EFAULT;
				break;
			}
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
			GSE_ERR("Perform calibration in suspend state!!\n");
			err = -EINVAL;
		} else {
			cali[ACCELHUB_AXIS_X] = sensor_data.x;
			cali[ACCELHUB_AXIS_Y] = sensor_data.y;
			cali[ACCELHUB_AXIS_Z] = sensor_data.z;
			err = accelhub_WriteCalibration(cali);
			if (err < 0)
				GSE_ERR("accelhub_WriteCalibration fail!!\n");
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		err = accelhub_ResetCalibration();
		if (err < 0)
			GSE_ERR("accelhub_ResetCalibration fail!!\n");
		break;

	case GSENSOR_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = accelhub_ReadCalibration(cali);
		if (err < 0) {
			GSE_ERR("accelhub_ResetCalibration fail!!\n");
			break;
		}
		sensor_data.x = cali[ACCELHUB_AXIS_X];
		sensor_data.y = cali[ACCELHUB_AXIS_Y];
		sensor_data.z = cali[ACCELHUB_AXIS_Z];
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}

	return err;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_accelhub_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	GSE_FUN();

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		GSE_ERR("compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case GSENSOR_IOCTL_SET_CALI:
	case GSENSOR_IOCTL_CLR_CALI:
	case GSENSOR_IOCTL_GET_CALI:
		ret = filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
		break;
	default:{
			GSE_ERR("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
			return -ENOIOCTLCMD;
		}
	}
	return ret;
}
#endif
static const struct file_operations accelhub_fops = {
	.open = accelhub_open,
	.release = accelhub_release,
	.unlocked_ioctl = accelhub_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_accelhub_unlocked_ioctl,
#endif
};

static struct miscdevice accelhub_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &accelhub_fops,
};

static int gsensor_open_report_data(int open)
{

	return 0;
}

static int gsensor_enable_nodata(int en)
{
	int err = 0;
	struct accelhub_ipi_data *obj = obj_ipi_data;

	if (atomic_read(&obj->scp_init_done)) {
		if (atomic_read(&obj->suspend) == 0) {
			err = accelhub_SetPowerMode(en);
			if (err < 0) {
				GSE_ERR("scp_gsensor_enable_nodata fail!\n");
				return -1;
			}
		}
	} else {
		GSE_ERR("sensor hub has not been ready!!\n");
		return -1;
	}

	GSE_LOG("scp_gsensor_enable_nodata OK!!!\n");
	return 0;
}

static int gsensor_set_delay(u64 ns)
{
	int err = 0;
	unsigned int delayms = 0;
	struct accelhub_ipi_data *obj = obj_ipi_data;

	delayms = (unsigned int)ns / 1000 / 1000;
	if (!atomic_read(&obj->scp_init_done)) {
		GSE_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_set_delay_to_hub(ID_ACCELEROMETER, delayms);
	if (err < 0) {
		GSE_ERR("gsensor_set_delay fail!\n");
		return err;
	}

	GSE_LOG("gsensor_set_delay (%d)\n", delayms);

	return 0;
}

static int gsensor_get_data(int *x, int *y, int *z, int *status)
{
	int err = 0;
	char buff[ACCELHUB_BUFSIZE];
	struct accelhub_ipi_data *obj = obj_ipi_data;

	err = accelhub_ReadSensorData(buff, ACCELHUB_BUFSIZE);
	if (err < 0) {
		GSE_ERR("accelhub_ReadSensorData fail!!\n");
		return -1;
	}
	sscanf(buff, "%x %x %x %x", x, y, z, status);

	if (atomic_read(&obj->trace) & ACCELHUB_TRC_RAWDATA)
		GSE_ERR("x = %d, y = %d, z = %d\n", *x, *y, *z);

	return 0;
}

static int scp_ready_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct accelhub_ipi_data *obj = obj_ipi_data;

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

static int accelhub_probe(struct platform_device *pdev)
{
	struct accelhub_ipi_data *obj;
	struct acc_control_path ctl = { 0 };
	struct acc_data_path data = { 0 };
	int err = 0;

	GSE_FUN();
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct accelhub_ipi_data));

	INIT_WORK(&obj->init_done_work, scp_init_work_done);

	obj_ipi_data = obj;

	platform_set_drvdata(pdev, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	atomic_set(&obj->scp_init_done, 0);
	atomic_set(&obj->first_ready_after_boot, 0);
	scp_register_notify(&scp_ready_notifier);
	err = SCP_sensorHub_rsp_registration(ID_ACCELEROMETER, gsensor_recv_interrput_ipidata);
	if (err < 0) {
		GSE_ERR("SCP_sensorHub_rsp_registration failed\n");
		goto exit_kfree;
	}
	err = misc_register(&accelhub_misc_device);
	if (err) {
		GSE_ERR("accelhub_misc_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	err = accelhub_create_attr(&accelhub_init_info.platform_diver_addr->driver);
	if (err) {
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = gsensor_open_report_data;
	ctl.enable_nodata = gsensor_enable_nodata;
	ctl.set_delay = gsensor_set_delay;
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;

	err = acc_register_control_path(&ctl);
	if (err) {
		GSE_ERR("register acc control path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = gsensor_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err) {
		GSE_ERR("register acc data path err\n");
		goto exit_create_attr_failed;
	}

	err = batch_register_support_info(ID_ACCELEROMETER, ctl.is_support_batch, data.vender_div, 1);
	if (err) {
		GSE_ERR("register gsensor batch support err = %d\n", err);
		goto exit_create_attr_failed;
	}
	gsensor_init_flag = 0;
	GSE_LOG("%s: OK\n", __func__);
	return 0;

 exit_create_attr_failed:
	accelhub_delete_attr(&(accelhub_init_info.platform_diver_addr->driver));
 exit_misc_device_register_failed:
	misc_deregister(&accelhub_misc_device);
 exit_kfree:
	kfree(obj);
 exit:
	GSE_ERR("%s: err = %d\n", __func__, err);
	gsensor_init_flag = -1;
	return err;
}

static int accelhub_remove(struct platform_device *pdev)
{
	int err = 0;

	err = accelhub_delete_attr(&accelhub_init_info.platform_diver_addr->driver);
	if (err)
		GSE_ERR("accelhub_delete_attr fail: %d\n", err);
	err = misc_deregister(&accelhub_misc_device);
	if (err)
		GSE_ERR("misc_deregister fail: %d\n", err);

	kfree(platform_get_drvdata(pdev));
	return 0;
}

static int accelhub_suspend(struct platform_device *pdev, pm_message_t msg)
{
	return 0;
}

static int accelhub_resume(struct platform_device *pdev)
{
	
	return 0;
}

static struct platform_device accelhub_device = {
	.name = ACCELHUB_DEV_NAME,
	.id = -1,
};

static struct platform_driver accelhub_driver = {
	.driver = {
		   .name = ACCELHUB_DEV_NAME,
		   },
	.probe = accelhub_probe,
	.remove = accelhub_remove,
	.suspend = accelhub_suspend,
	.resume = accelhub_resume,
};

static int gsensor_local_init(void)
{
	GSE_FUN();

	if (platform_driver_register(&accelhub_driver)) {
		GSE_ERR("add driver error\n");
		return -1;
	}
	if (-1 == gsensor_init_flag)
		return -1;
	return 0;
}

static int gsensor_local_remove(void)
{
	GSE_FUN();
	platform_driver_unregister(&accelhub_driver);
	return 0;
}

static struct acc_init_info accelhub_init_info = {
	.name = "accelhub",
	.init = gsensor_local_init,
	.uninit = gsensor_local_remove,
};

static int __init accelhub_init(void)
{

	if (platform_device_register(&accelhub_device)) {
		GSE_ERR("accel platform device error\n");
		return -1;
	}
	acc_driver_add(&accelhub_init_info);
	return 0;
}

static void __exit accelhub_exit(void)
{
	GSE_FUN();
}

module_init(accelhub_init);
module_exit(accelhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACCELHUB gse driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
