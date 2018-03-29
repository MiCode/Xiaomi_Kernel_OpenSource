/*
* Copyright(C)2014 MediaTek Inc.
* Modification based on code covered by the below mentioned copyright
* and/or permission notice(S).
*/

/* maghub.c - maghub compass driver
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
#include <hwmsensor.h>
#include "maghub.h"
#include "mag.h"
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define MAGHUB_DEV_NAME         "mag_hub"
#define DRIVER_VERSION          "1.0.1"
#define MAGHUB_DEBUG		1

#if MAGHUB_DEBUG
#define MAGN_TAG                  "[Msensor] "
#define MAGN_FUN(f)               pr_err(MAGN_TAG"%s\n", __func__)
#define MAGN_ERR(fmt, args...)    pr_err(MAGN_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define MAGN_LOG(fmt, args...)    pr_err(MAGN_TAG fmt, ##args)
#else
#define MAGN_TAG
#define MAGN_FUN(f)               do {} while (0)
#define MAGN_ERR(fmt, args...)    do {} while (0)
#define MAGN_LOG(fmt, args...)    do {} while (0)
#endif


struct maghub_ipi_data *mag_ipi_data;
static struct mag_init_info maghub_init_info;

static int maghub_init_flag = -1;

typedef enum {
	MAG_FUN_DEBUG = 0x01,
	MAG_MDATA_DEBUG = 0X02,
	MAG_ODATA_DEBUG = 0X04,
	MAG_CTR_DEBUG = 0X08,
	MAG_IPI_DEBUG = 0x10,
} MAG_TRC;
struct maghub_ipi_data {
	int		direction;
	atomic_t	trace;
	atomic_t	suspend;
	atomic_t	scp_init_done;
	struct work_struct init_done_work;
	struct data_unit_t m_data_t;
};
static int maghub_m_setPowerMode(bool enable)
{
	int res = 0;
	struct maghub_ipi_data *obj = mag_ipi_data;

	MAGN_LOG("magnetic enable value = %d\n", enable);

	if (!atomic_read(&obj->scp_init_done)) {
		MAGN_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	if (atomic_read(&obj->scp_init_done)) {
		res = sensor_enable_to_hub(ID_MAGNETIC, enable);
		if (res < 0)
			MAGN_ERR("maghub_m_setPowerMode is failed!!\n");

	} else {
		MAGN_ERR("sensor hub has not been ready!!\n");
	}

	return res;
}

static int maghub_GetMData(char *buf, int size)
{
	struct maghub_ipi_data *obj = mag_ipi_data;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;
	int mag_m[MAGHUB_AXES_NUM];
	int err = 0;
	unsigned int status = 0;

	if (!atomic_read(&obj->scp_init_done)) {
		MAGN_ERR("sensor hub has not been ready!!\n");
		return -1;
	}

	if (atomic_read(&obj->suspend))
		return -3;

	if (NULL == buf)
		return -1;
	err = sensor_get_data_from_hub(ID_MAGNETIC, &data);
	if (err < 0) {
		MAGN_ERR("sensor_get_data_from_hub fail!\n");
		return err;
	}

	time_stamp				= data.time_stamp;
	time_stamp_gpt			= data.time_stamp_gpt;
	mag_m[MAGHUB_AXIS_X]	= data.magnetic_t.x;
	mag_m[MAGHUB_AXIS_Y]	= data.magnetic_t.y;
	mag_m[MAGHUB_AXIS_Z]	= data.magnetic_t.z;
	status					= data.magnetic_t.status;

	MAGN_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, x: %d, y: %d, z: %d!\n", time_stamp, time_stamp_gpt,
		mag_m[MAGHUB_AXIS_X], mag_m[MAGHUB_AXIS_Y], mag_m[MAGHUB_AXIS_Z]);


	sprintf(buf, "%04x %04x %04x %04x", mag_m[MAGHUB_AXIS_X], mag_m[MAGHUB_AXIS_Y], mag_m[MAGHUB_AXIS_Z], status);

	if (atomic_read(&obj->trace) & MAG_MDATA_DEBUG)
		MAGN_ERR("RAW DATA: %s!\n", buf);


	return 0;
}

static int maghub_ReadChipInfo(char *buf, int bufsize)
{
	if ((!buf) || (bufsize <= MAGHUB_BUFSIZE - 1))
		return -1;

	sprintf(buf, "maghub Chip");
	return 0;
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[MAGHUB_BUFSIZE];

	maghub_ReadChipInfo(strbuf, MAGHUB_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[MAGHUB_BUFSIZE];

	maghub_m_setPowerMode(true);
	msleep(20);
	maghub_GetMData(strbuf, MAGHUB_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct maghub_ipi_data *obj = mag_ipi_data;

	if (NULL == obj) {
		MAGN_ERR("maghub_ipi_data is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct maghub_ipi_data *obj = mag_ipi_data;
	int trace = 0;
	int res = 0;

	if (NULL == obj) {
		MAGN_ERR("maghub_ipi_data is null!!\n");
		return 0;
	}
	if (!atomic_read(&obj->scp_init_done)) {
		MAGN_ERR("sensor hub has not been ready!!\n");
		return 0;
	}
	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
		res = sensor_set_cmd_to_hub(ID_MAGNETIC, CUST_ACTION_SET_TRACE, &trace);
		if (res < 0) {
			MAGN_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
				ID_MAGNETIC, CUST_ACTION_SET_TRACE);
			return 0;
		}
	} else {
		MAGN_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}
	return count;
}

static ssize_t show_chip_orientation(struct device_driver *ddri, char *buf)
{
	ssize_t _tLength = 0;
	struct maghub_ipi_data *obj = mag_ipi_data;

	MAGN_LOG("[%s] default direction: %d\n", __func__, obj->direction);

	_tLength = snprintf(buf, PAGE_SIZE, "default direction = %d\n", obj->direction);

	return _tLength;
}

static ssize_t store_chip_orientation(struct device_driver *ddri, const char *buf, size_t tCount)
{
	int _nDirection = 0, err = 0;
	struct maghub_ipi_data *obj = mag_ipi_data;
	int res = 0;

	if (NULL == obj)
		return 0;
	if (!atomic_read(&obj->scp_init_done)) {
		MAGN_ERR("sensor hub has not been ready!!\n");
		return 0;
	}
	err = kstrtoint(buf, 10, &_nDirection);
	if (err == 0) {
		obj->direction = _nDirection;
		res = sensor_set_cmd_to_hub(ID_MAGNETIC, CUST_ACTION_SET_DIRECTION, &_nDirection);
		if (res < 0) {
			MAGN_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
				ID_MAGNETIC, CUST_ACTION_SET_DIRECTION);
			return 0;
		}
	}

	MAGN_LOG("[%s] set direction: %d\n", __func__, _nDirection);

	return tCount;
}

static ssize_t show_regiter_map(struct device_driver *ddri, char *buf)
{

	ssize_t _tLength = 0;

	return _tLength;
}
static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IRUGO | S_IWUSR, show_trace_value, store_trace_value);
static DRIVER_ATTR(orientation, S_IWUSR | S_IRUGO, show_chip_orientation, store_chip_orientation);
static DRIVER_ATTR(regmap, S_IRUGO, show_regiter_map, NULL);
static struct driver_attribute *maghub_attr_list[] = {
	&driver_attr_chipinfo,
	&driver_attr_sensordata,
	&driver_attr_trace,
	&driver_attr_orientation,
	&driver_attr_regmap,
};
static int maghub_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(maghub_attr_list) / sizeof(maghub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, maghub_attr_list[idx]);
		if (err) {
			MAGN_ERR("driver_create_file (%s) = %d\n", maghub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
static int maghub_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(maghub_attr_list) / sizeof(maghub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, maghub_attr_list[idx]);

	return err;
}
static int mag_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		err = mag_flush_report();
	else if (event->flush_action == DATA_ACTION)
		err = mag_data_report(event->magnetic_t.x, event->magnetic_t.y, event->magnetic_t.z,
			event->magnetic_t.status, (int64_t)(event->time_stamp + event->time_stamp_gpt));
	else if (event->flush_action == BIAS_ACTION)
		err = mag_bias_report(event->magnetic_t.x_bias, event->magnetic_t.y_bias, event->magnetic_t.z_bias);
	return err;
}

static int maghub_open(struct inode *inode, struct file *file)
{
	struct maghub_ipi_data *obj = mag_ipi_data;
	int ret = -1;

	if (atomic_read(&obj->trace) & MAG_CTR_DEBUG)
		MAGN_LOG("Open device node:maghub\n");
	ret = nonseekable_open(inode, file);

	return ret;
}
static int maghub_release(struct inode *inode, struct file *file)
{
	struct maghub_ipi_data *obj = mag_ipi_data;

	if (atomic_read(&obj->trace) & MAG_CTR_DEBUG)
		MAGN_LOG("Release device node:maghub\n");
	return 0;
}
static long maghub_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	char buff[MAGHUB_BUFSIZE];	/* for chip information */
	long ret = -1;		/* Return value. */
	uint32_t enable = 0;

	switch (cmd) {
	case MSENSOR_IOCTL_READ_CHIPINFO:
		if (argp == NULL) {
			MAGN_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}

		ret = maghub_ReadChipInfo(buff, MAGHUB_BUFSIZE);
		if (copy_to_user(argp, buff, strlen(buff) + 1))
			return -EFAULT;
		break;

	case MSENSOR_IOCTL_READ_SENSORDATA:
		if (argp == NULL) {
			MAGN_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}

		ret = maghub_GetMData(buff, MAGHUB_BUFSIZE);
		if (ret < 0) {
			MAGN_ERR("maghub_GetMData fail!\r\n");
			break;
		}

		if (copy_to_user(argp, buff, strlen(buff) + 1))
			return -EFAULT;
		break;

	case MSENSOR_IOCTL_SENSOR_ENABLE:

		if (argp == NULL) {
			MAGN_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}
		if (copy_from_user(&enable, argp, sizeof(enable))) {
			MAGN_LOG("copy_from_user failed.");
			return -EFAULT;
		}
		MAGN_LOG("MSENSOR_IOCTL_SENSOR_ENABLE enable=%d!\r\n", enable);
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
		ret = maghub_m_setPowerMode(enable);
		if (ret < 0)
			MAGN_ERR("maghub_m_enable fail!\r\n");
		if (enable == 1) {
			sensor_set_delay_to_hub(ID_MAGNETIC, 100);
			if (ret < 0)
				MAGN_ERR("sensor_set_delay_to_hub fail!\r\n");
		}
#elif defined CONFIG_NANOHUB
		if (enable == 1) {
			sensor_set_delay_to_hub(ID_MAGNETIC, 20);
			if (ret < 0)
				MAGN_ERR("sensor_set_delay_to_hub fail!\r\n");
		}
		ret = maghub_m_setPowerMode(enable);
		if (ret < 0)
			MAGN_ERR("maghub_m_enable fail!\r\n");
#else

#endif
		break;

	case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
		if (argp == NULL) {
			MAGN_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}
		if (copy_to_user(argp, buff, strlen(buff) + 1))
			return -EFAULT;

		break;

	default:
		MAGN_ERR("%s not supported = 0x%04x", __func__, cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long maghub_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {

	case COMPAT_MSENSOR_IOCTL_READ_CHIPINFO:
		ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_CHIPINFO, (unsigned long)arg32);
		if (ret) {
			MAGN_LOG("MSENSOR_IOCTL_READ_CHIPINFO unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_MSENSOR_IOCTL_READ_SENSORDATA:
		ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_SENSORDATA, (unsigned long)arg32);
		if (ret) {
			MAGN_LOG("MSENSOR_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_MSENSOR_IOCTL_SENSOR_ENABLE:
		if (arg32 == NULL) {
			MAGN_LOG("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SENSOR_ENABLE, (unsigned long)(arg32));
		if (ret) {
			MAGN_LOG("MSENSOR_IOCTL_SENSOR_ENABLE unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
		if (arg32 == NULL) {
			MAGN_LOG("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_FACTORY_SENSORDATA, (unsigned long)(arg32));
		if (ret) {
			MAGN_LOG("MSENSOR_IOCTL_READ_FACTORY_SENSORDATA unlocked_ioctl failed.");
			return ret;
		}
		break;

	default:
		MAGN_LOG("%s not supported = 0x%04x", __func__, cmd);
		ret =  -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif

static const struct file_operations maghub_fops = {
	.owner = THIS_MODULE,
	.open = maghub_open,
	.release = maghub_release,
	.unlocked_ioctl = maghub_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = maghub_compat_ioctl,
#endif
};

static struct miscdevice maghub_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msensor",
	.fops = &maghub_fops,
};
static int maghub_enable(int en)
{
	int res = 0;

	res = maghub_m_setPowerMode(en);
	if (res)
		MAGN_ERR("maghub_m_setPowerMode is failed!!\n");
	return res;
}

static int maghub_set_delay(u64 ns)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	int delayms = 0, err = 0;
	struct maghub_ipi_data *obj = mag_ipi_data;

	delayms = (int)ns / 1000 / 1000;
	if (!atomic_read(&obj->scp_init_done)) {
		MAGN_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_set_delay_to_hub(ID_MAGNETIC, delayms);
	if (err < 0) {
		MAGN_ERR("maghub_m_set_delay fail!\n");
		return err;
	}

	MAGN_LOG("maghub_m_set_delay (%d)\n", delayms);
	return err;
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int maghub_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_MAGNETIC, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int maghub_flush(void)
{
	return sensor_flush_to_hub(ID_MAGNETIC);
}

static int maghub_set_cali(uint8_t *data, uint8_t count)
{
	return sensor_cfg_to_hub(ID_MAGNETIC, data, count);
}

static int maghub_open_report_data(int open)
{
	return 0;
}

static int maghub_get_data(int *x, int *y, int *z, int *status)
{
	char buff[MAGHUB_BUFSIZE];

	maghub_GetMData(buff, MAGHUB_BUFSIZE);

	if (4 != sscanf(buff, "%x %x %x %x", x, y, z, status))
		MAGN_ERR("maghub_m_get_data sscanf fail!!\n");
	return 0;
}
static int scp_ready_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct maghub_ipi_data *obj = mag_ipi_data;

	switch (event) {
	case SCP_EVENT_READY:
	    atomic_set(&obj->scp_init_done, 1);
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
static int maghub_probe(struct platform_device *pdev)
{
	int err = 0;
	struct maghub_ipi_data *data;
	struct mag_control_path ctl = { 0 };
	struct mag_data_path mag_data = { 0 };

	MAGN_FUN();
	data = kzalloc(sizeof(struct maghub_ipi_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	mag_ipi_data = data;
	atomic_set(&data->trace, 0);

	platform_set_drvdata(pdev, data);

	scp_register_notify(&scp_ready_notifier);
	err = SCP_sensorHub_data_registration(ID_MAGNETIC, mag_recv_data);
	if (err < 0) {
		MAGN_ERR("SCP_sensorHub_data_registration failed\n");
		goto exit_kfree;
	}
	err = misc_register(&maghub_misc_device);
	if (err) {
		MAGN_ERR("maghub_misc_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	/* Register sysfs attribute */
	err = maghub_create_attr(&(maghub_init_info.platform_diver_addr->driver));
	if (err) {
		MAGN_ERR("create attribute err = %d\n", err);
		goto create_attr_failed;
	}
	ctl.is_use_common_factory = false;
	ctl.enable = maghub_enable;
	ctl.set_delay = maghub_set_delay;
	ctl.batch = maghub_batch;
	ctl.flush = maghub_flush;
	ctl.set_cali = maghub_set_cali;
	ctl.open_report_data = maghub_open_report_data;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#else
#endif

	err = mag_register_control_path(&ctl);
	if (err) {
		MAGN_ERR("register mag control path err\n");
		goto create_attr_failed;
	}

	mag_data.div = CONVERT_M_DIV;
	mag_data.get_data = maghub_get_data;

	err = mag_register_data_path(&mag_data);
	if (err) {
		MAGN_ERR("register data control path err\n");
		goto create_attr_failed;
	}
	MAGN_ERR("%s: OK\n", __func__);
	maghub_init_flag = 1;
	return 0;

create_attr_failed:
	maghub_delete_attr(&(maghub_init_info.platform_diver_addr->driver));
exit_misc_device_register_failed:
	misc_deregister(&maghub_misc_device);
exit_kfree:
	kfree(data);
exit:
	MAGN_ERR("%s: err = %d\n", __func__, err);
	maghub_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int maghub_remove(struct platform_device *pdev)
{
	int err = 0;

	err = maghub_delete_attr(&(maghub_init_info.platform_diver_addr->driver));
	if (err)
		MAGN_ERR("maghub_delete_attr fail: %d\n", err);

	kfree(platform_get_drvdata(pdev));
	misc_deregister(&maghub_misc_device);
	return 0;
}

static int maghub_suspend(struct platform_device *pdev, pm_message_t msg)
{
	return 0;
}

static int maghub_resume(struct platform_device *pdev)
{
	return 0;
}
static struct platform_device maghub_device = {
	.name = MAGHUB_DEV_NAME,
	.id = -1,
};

static struct platform_driver maghub_driver = {
	.driver = {
		   .name = MAGHUB_DEV_NAME,
	},
	.probe = maghub_probe,
	.remove = maghub_remove,
	.suspend = maghub_suspend,
	.resume = maghub_resume,
};

static int maghub_local_remove(void)
{
	platform_driver_unregister(&maghub_driver);
	return 0;
}

static int maghub_local_init(void)
{
	if (platform_driver_register(&maghub_driver)) {
		MAGN_ERR("add_driver error\n");
		return -1;
	}
	if (-1 == maghub_init_flag)
		return -1;
	return 0;
}
static struct mag_init_info maghub_init_info = {
	.name = "maghub",
	.init = maghub_local_init,
	.uninit = maghub_local_remove,
};

static int __init maghub_init(void)
{
	if (platform_device_register(&maghub_device)) {
		MAGN_ERR("platform device error\n");
		return -1;
	}
	mag_driver_add(&maghub_init_info);
	return 0;
}

static void __exit maghub_exit(void)
{
	MAGN_FUN();
}

module_init(maghub_init);
module_exit(maghub_exit);

MODULE_AUTHOR("hongxu.zhao@mediatek.com");
MODULE_DESCRIPTION("MAGHUB compass driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
