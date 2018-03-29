/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * History: V1.0 --- [2013.03.14]Driver creation
 *          V1.1 --- [2013.07.03]Re-write I2C function to fix the bug that
 *                               i2c access error on MT6589 platform.
 *          V1.2 --- [2013.07.04]Add self test function.
 *          V1.3 --- [2013.07.04]Support new chip id 0x57 and 0x58.
 */
#include "barohub.h"
#include <barometer.h>
#include <hwmsensor.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

/* trace */
enum BAR_TRC {
	BAR_TRC_READ = 0x01,
	BAR_TRC_RAWDATA = 0x02,
	BAR_TRC_IOCTL = 0x04,
	BAR_TRC_FILTER = 0x08,
};

/* barohub i2c client data */
struct barohub_ipi_data {
	/* sensor info */
	atomic_t trace;
	atomic_t suspend;
	struct work_struct init_done_work;
	atomic_t scp_init_done;
};

#define BAR_TAG                  "[barometer] "
#define BAR_FUN(f)               pr_err(BAR_TAG"%s\n", __func__)
#define BAR_ERR(fmt, args...) \
	pr_err(BAR_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define BAR_LOG(fmt, args...)    pr_err(BAR_TAG fmt, ##args)

static struct barohub_ipi_data *obj_ipi_data;
static int barohub_local_init(void);
static int barohub_local_remove(void);
static int barohub_init_flag = -1;
static struct baro_init_info barohub_init_info = {
	.name = "barohub",
	.init = barohub_local_init,
	.uninit = barohub_local_remove,
};

static int barohub_set_powermode(bool enable)
{
	int err = 0;
	struct barohub_ipi_data *obj = obj_ipi_data;

	if (!atomic_read(&obj->scp_init_done)) {
		BAR_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_enable_to_hub(ID_PRESSURE, enable);
	if (err < 0)
		BAR_ERR("SCP_sensorHub_req_send fail!\n");

	return err;
}

/*
*get compensated temperature
*unit:10 degrees centigrade
*/
static int barohub_get_temperature(char *buf, int bufsize)
{
	int err = 0;

	return err;
}

/*
*get compensated pressure
*unit: hectopascal(hPa)
*/
static int barohub_get_pressure(char *buf, int bufsize)
{
	struct barohub_ipi_data *obj = obj_ipi_data;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;
	int pressure;
	int err = 0;

	if (!atomic_read(&obj->scp_init_done)) {
		BAR_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	if (atomic_read(&obj->suspend))
		return -3;

	if (NULL == buf)
		return -1;
	err = sensor_get_data_from_hub(ID_PRESSURE, &data);
	if (err < 0) {
		BAR_ERR("sensor_get_data_from_hub fail!\n");
		return err;
	}

	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	pressure		= data.pressure_t.pressure;

	/* BAR_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, pressure: %d!\n",
	time_stamp, time_stamp_gpt, pressure); */

	sprintf(buf, "%08x", pressure);
	if (atomic_read(&obj->trace) & BAR_TRC_IOCTL)
		BAR_LOG("compensated pressure value: %s\n", buf);

	return err;
}
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[BAROHUB_BUFSIZE];
	int err = 0;

	err = barohub_set_powermode(true);
	if (err < 0) {
		BAR_ERR("barohub_set_powermode fail!!\n");
		return 0;
	}
	err = barohub_get_pressure(strbuf, BAROHUB_BUFSIZE);
	if (err < 0) {
		BAR_ERR("barohub_set_powermode fail!!\n");
		return 0;
	}
	return sprintf(buf, "%s\n", strbuf);
}
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct barohub_ipi_data *obj = obj_ipi_data;

	if (obj == NULL) {
		BAR_ERR("pointer is null\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct barohub_ipi_data *obj = obj_ipi_data;
	int trace = 0, res = 0;

	if (obj == NULL) {
		BAR_ERR("obj is null\n");
		return 0;
	}
	if (!atomic_read(&obj->scp_init_done)) {
		BAR_ERR("sensor hub has not been ready!!\n");
		return 0;
	}
	res = kstrtoint(buf, 10, &trace);
	if (res == 0) {
		atomic_set(&obj->trace, trace);
		res = sensor_set_cmd_to_hub(ID_PRESSURE, CUST_ACTION_SET_TRACE, &trace);
		if (res < 0) {
			BAR_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
				ID_PRESSURE, CUST_ACTION_SET_TRACE);
			return 0;
		}
	} else {
		BAR_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}
	return count;
}
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);

static struct driver_attribute *barohub_attr_list[] = {
	&driver_attr_sensordata,	/* dump sensor data */
	&driver_attr_trace,	/* trace log */
};

static int barohub_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(barohub_attr_list) / sizeof(barohub_attr_list[0]));

	if (NULL == driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, barohub_attr_list[idx]);
		if (err) {
			BAR_ERR("driver_create_file (%s) = %d\n", barohub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int barohub_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(barohub_attr_list) / sizeof(barohub_attr_list[0]));

	if (NULL == driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, barohub_attr_list[idx]);

	return err;
}

static int barohub_open(struct inode *inode, struct file *file)
{
	file->private_data = obj_ipi_data;

	if (file->private_data == NULL) {
		BAR_ERR("null pointer\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int barohub_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long barohub_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	char strbuf[BAROHUB_BUFSIZE];
	u32 dat = 0;
	void __user *data;
	int err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		BAR_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case BAROMETER_IOCTL_INIT:
		err = barohub_set_powermode(true);
		if (err < 0) {
			err = -EFAULT;
			break;
		}
		err = sensor_set_delay_to_hub(ID_PRESSURE, 200);
		if (err) {
			BAR_ERR("sensor_set_delay_to_hub failed!\n");
			break;
		}
		break;

	case BAROMETER_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}
		strcpy(strbuf, "baro_hub");
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case BAROMETER_GET_PRESS_DATA:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}

		err = barohub_get_pressure(strbuf, BAROHUB_BUFSIZE);
		if (err < 0) {
			BAR_ERR("barohub_get_pressure fail\n");
			break;
		}
		err = kstrtoint(strbuf, 16, &dat);
		if (err == 0) {
			if (copy_to_user(data, &dat, sizeof(dat))) {
				err = -EFAULT;
				break;
			}
		}
		break;

	case BAROMETER_GET_TEMP_DATA:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}
		err = barohub_get_temperature(strbuf, BAROHUB_BUFSIZE);
		if (err < 0) {
			BAR_ERR("barohub_get_temperature fail\n");
			break;
		}
		dat = 0;
		if (copy_to_user(data, &dat, sizeof(dat))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		BAR_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}

static const struct file_operations barohub_fops = {
	.owner = THIS_MODULE,
	.open = barohub_open,
	.release = barohub_release,
	.unlocked_ioctl = barohub_unlocked_ioctl,
};

static struct miscdevice barohub_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "barometer",
	.fops = &barohub_fops,
};
static int barohub_open_report_data(int open)
{
	return 0;
}

static int barohub_enable_nodata(int en)
{
	int res = 0;
	bool power = false;

	if (1 == en)
		power = true;
	if (0 == en)
		power = false;

	res = barohub_set_powermode(power);
	if (res < 0) {
		BAR_LOG("barohub_set_powermode fail\n");
		return res;
	}
	BAR_LOG("barohub_set_powermode OK!\n");
	return res;
}

static int barohub_set_delay(u64 ns)
{
	int err = 0;
	unsigned int delayms = 0;
	struct barohub_ipi_data *obj = obj_ipi_data;

	delayms = (unsigned int)ns / 1000 / 1000;
	if (atomic_read(&obj->scp_init_done)) {
		err = sensor_set_delay_to_hub(ID_PRESSURE, delayms);
		if (err < 0) {
			BAR_ERR("als_set_delay fail!\n");
			return err;
		}
	} else {
		BAR_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	BAR_LOG("barohub_set_delay (%d)\n", delayms);

	return 0;
}

static int barohub_get_data(int *value, int *status)
{
	char buff[BAROHUB_BUFSIZE];
	int err = 0;

	err = barohub_get_pressure(buff, BAROHUB_BUFSIZE);
	if (err) {
		BAR_ERR("get compensated pressure value failed," "err = %d\n", err);
		return -1;
	}
	err = kstrtoint(buff, 16, value);
	if (err == 0)
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}
static int scp_ready_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct barohub_ipi_data *obj = obj_ipi_data;

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

static int barohub_probe(struct platform_device *pdev)
{
	struct barohub_ipi_data *obj;
	struct baro_control_path ctl = { 0 };
	struct baro_data_path data = { 0 };
	int err = 0;

	BAR_FUN();

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	obj_ipi_data = obj;
	platform_set_drvdata(pdev, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

	atomic_set(&obj->scp_init_done, 0);
	scp_register_notify(&scp_ready_notifier);
	err = misc_register(&barohub_misc_device);
	if (err) {
		BAR_ERR("misc device register failed, err = %d\n", err);
		goto exit_misc_device_register_failed;
	}

	ctl.is_use_common_factory = false;
	err = barohub_create_attr(&(barohub_init_info.platform_diver_addr->driver));
	if (err) {
		BAR_ERR("create attribute failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = barohub_open_report_data;
	ctl.enable_nodata = barohub_enable_nodata;
	ctl.set_delay = barohub_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = false;

	err = baro_register_control_path(&ctl);
	if (err) {
		BAR_ERR("register baro control path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = barohub_get_data;
	data.vender_div = 100;
	err = baro_register_data_path(&data);
	if (err) {
		BAR_ERR("baro_register_data_path failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}
	err = batch_register_support_info(ID_PRESSURE, ctl.is_support_batch, data.vender_div, 1);
	if (err) {
		BAR_ERR("register baro batch support err = %d\n", err);
		goto exit_create_attr_failed;
	}

	barohub_init_flag = 0;
	BAR_LOG("%s: OK\n", __func__);
	return 0;


exit_create_attr_failed:
	barohub_delete_attr(&(barohub_init_info.platform_diver_addr->driver));
exit_misc_device_register_failed:
	misc_deregister(&barohub_misc_device);
	kfree(obj);
exit:
	BAR_ERR("err = %d\n", err);
	barohub_init_flag = -1;
	return err;
}

static int barohub_remove(struct platform_device *pdev)
{
	int err = 0;

	err = barohub_delete_attr(&(barohub_init_info.platform_diver_addr->driver));
	if (err)
		BAR_ERR("barohub_delete_attr failed, err = %d\n", err);

	err = misc_deregister(&barohub_misc_device);
	if (err)
		BAR_ERR("misc_deregister failed, err = %d\n", err);

	obj_ipi_data = NULL;
	kfree(platform_get_drvdata(pdev));

	return 0;
}
static int barohub_suspend(struct platform_device *pdev, pm_message_t msg)
{
	return 0;
}

static int barohub_resume(struct platform_device *pdev)
{
	return 0;
}
static struct platform_device barohub_device = {
	.name = BAROHUB_DEV_NAME,
	.id = -1,
};

static struct platform_driver barohub_driver = {
	.driver = {
		.name = BAROHUB_DEV_NAME,
	},
	.probe = barohub_probe,
	.remove = barohub_remove,
	.suspend = barohub_suspend,
	.resume = barohub_resume,
};

static int barohub_local_remove(void)
{
	BAR_FUN();
	platform_driver_unregister(&barohub_driver);
	return 0;
}

static int barohub_local_init(void)
{
	if (platform_driver_register(&barohub_driver)) {
		BAR_ERR("add driver error\n");
		return -1;
	}
	if (-1 == barohub_init_flag)
		return -1;
	return 0;
}

static int __init barohub_init(void)
{
	BAR_FUN();
	if (platform_device_register(&barohub_device)) {
		BAR_ERR("baro platform device error\n");
		return -1;
	}
	baro_driver_add(&barohub_init_info);
	return 0;
}

static void __exit barohub_exit(void)
{
	BAR_FUN();
	platform_driver_unregister(&barohub_driver);
}

module_init(barohub_init);
module_exit(barohub_exit);

MODULE_LICENSE("GPLv2");
MODULE_DESCRIPTION("BAROHUB Driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
MODULE_VERSION(BAROHUB_DRIVER_VERSION);
