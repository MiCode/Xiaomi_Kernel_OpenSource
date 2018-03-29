/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <hwmsensor.h>
#include "hmdyhub.h"
#include <humidity.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

/* trace */
enum HMDY_TRC {
	HMDY_TRC_READ = 0x01,
	HMDY_TRC_RAWDATA = 0x02,
	HMDY_TRC_IOCTL = 0x04,
	HMDY_TRC_FILTER = 0x08,
};

/* hmdyhub i2c client data */
struct hmdyhub_ipi_data {
	/* sensor info */
	atomic_t trace;
	atomic_t suspend;
	struct work_struct init_done_work;
	atomic_t scp_init_done;
};

#define HMDYHUB_TAG                  "[humidity] "
#define HMDYHUB_FUN(f)               pr_err(HMDYHUB_TAG"%s\n", __func__)
#define HMDYHUB_ERR(fmt, args...) \
	pr_err(HMDY_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define HMDYHUB_LOG(fmt, args...)    pr_err(HMDYHUB_TAG fmt, ##args)

static struct hmdyhub_ipi_data *obj_ipi_data;
static int hmdyhub_local_init(void);
static int hmdyhub_local_remove(void);
static int hmdyhub_init_flag = -1;
static struct hmdy_init_info hmdyhub_init_info = {
	.name = "hmdyhub",
	.init = hmdyhub_local_init,
	.uninit = hmdyhub_local_remove,
};

static int hmdyhub_set_powermode(bool enable)
{
	int err = 0;
	struct hmdyhub_ipi_data *obj = obj_ipi_data;

	if (!atomic_read(&obj->scp_init_done)) {
		HMDYHUB_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	err = sensor_enable_to_hub(ID_RELATIVE_HUMIDITY, enable);
	if (err < 0)
		HMDYHUB_ERR("sensor_enable_to_hub fail!\n");

	return err;
}

/*
*get compensated temperature
*unit:10 degrees centigrade
*/
static int hmdyhub_get_temperature(char *buf, int bufsize)
{
	int err = 0;

	return err;
}

/*
*get compensated humidity
*unit: hectopascal(hPa)
*/
static int hmdyhub_get_humidity(char *buf, int bufsize)
{
	struct hmdyhub_ipi_data *obj = obj_ipi_data;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;
	int humidity;
	int err = 0;

	if (!atomic_read(&obj->scp_init_done)) {
		HMDYHUB_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	if (atomic_read(&obj->suspend))
		return -3;

	if (NULL == buf)
		return -1;
	err = sensor_get_data_from_hub(ID_RELATIVE_HUMIDITY, &data);
	if (err < 0) {
		HMDYHUB_ERR("sensor_get_data_from_hub fail!\n");
		return err;
	}
	time_stamp = data.time_stamp;
	time_stamp_gpt = data.time_stamp_gpt;
	humidity = data.relative_humidity_t.relative_humidity;

	HMDYHUB_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, humidity: %d!\n", time_stamp, time_stamp_gpt,
			humidity);

	sprintf(buf, "%08x", humidity);
	if (atomic_read(&obj->trace) & HMDY_TRC_IOCTL)
		HMDYHUB_LOG("compensated humidity value: %s\n", buf);

	return err;
}
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[HMDYHUB_BUFSIZE];
	int err = 0;

	err = hmdyhub_set_powermode(true);
	if (err < 0) {
		HMDYHUB_ERR("hmdyhub_set_powermode fail!!\n");
		return 0;
	}
	err = hmdyhub_get_humidity(strbuf, HMDYHUB_BUFSIZE);
	if (err < 0) {
		HMDYHUB_ERR("hmdyhub_set_powermode fail!!\n");
		return 0;
	}
	return sprintf(buf, "%s\n", strbuf);
}
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct hmdyhub_ipi_data *obj = obj_ipi_data;

	if (obj == NULL) {
		HMDYHUB_ERR("pointer is null\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct hmdyhub_ipi_data *obj = obj_ipi_data;
	int trace = 0, res = 0;

	if (obj == NULL) {
		HMDYHUB_ERR("obj is null\n");
		return 0;
	}
	if (!atomic_read(&obj->scp_init_done)) {
		HMDYHUB_ERR("sensor hub has not been ready!!\n");
		return 0;
	}
	res = kstrtoint(buf, 10, &trace);
	if (res == 0) {
		atomic_set(&obj->trace, trace);
		res = sensor_set_cmd_to_hub(ID_RELATIVE_HUMIDITY, CUST_ACTION_SET_TRACE, &trace);
		if (res < 0) {
			HMDYHUB_ERR("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
				ID_RELATIVE_HUMIDITY, CUST_ACTION_SET_TRACE);
			return 0;
		}
	} else {
		HMDYHUB_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}
	return count;
}
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);

static struct driver_attribute *hmdyhub_attr_list[] = {
	&driver_attr_sensordata,	/* dump sensor data */
	&driver_attr_trace,	/* trace log */
};

static int hmdyhub_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(hmdyhub_attr_list) / sizeof(hmdyhub_attr_list[0]));

	if (NULL == driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, hmdyhub_attr_list[idx]);
		if (err) {
			HMDYHUB_ERR("driver_create_file (%s) = %d\n", hmdyhub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int hmdyhub_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(hmdyhub_attr_list) / sizeof(hmdyhub_attr_list[0]));

	if (NULL == driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, hmdyhub_attr_list[idx]);

	return err;
}

static int hmdyhub_open(struct inode *inode, struct file *file)
{
	file->private_data = obj_ipi_data;

	if (file->private_data == NULL) {
		HMDYHUB_ERR("null pointer\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int hmdyhub_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long hmdyhub_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	char strbuf[HMDYHUB_BUFSIZE];
	u32 dat = 0;
	void __user *data;
	int err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		HMDYHUB_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case HUMIDITY_IOCTL_INIT:
		err = hmdyhub_set_powermode(true);
		if (err < 0) {
			err = -EFAULT;
			break;
		}
		err = sensor_set_delay_to_hub(ID_RELATIVE_HUMIDITY, 200);
		if (err) {
			HMDYHUB_ERR("sensor_set_delay_to_hub failed!\n");
			break;
		}
		break;

	case HUMIDITY_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}
		strcpy(strbuf, "hmdy_hub");
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case HUMIDITY_GET_HMDY_DATA:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}

		err = hmdyhub_get_humidity(strbuf, HMDYHUB_BUFSIZE);
		if (err < 0) {
			HMDYHUB_ERR("hmdyhub_get_humidity fail\n");
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

	case HUMIDITY_GET_TEMP_DATA:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}
		err = hmdyhub_get_temperature(strbuf, HMDYHUB_BUFSIZE);
		if (err < 0) {
			HMDYHUB_ERR("hmdyhub_get_temperature fail\n");
			break;
		}
		dat = 0;
		if (copy_to_user(data, &dat, sizeof(dat))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		HMDYHUB_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}

static const struct file_operations hmdyhub_fops = {
	.owner = THIS_MODULE,
	.open = hmdyhub_open,
	.release = hmdyhub_release,
	.unlocked_ioctl = hmdyhub_unlocked_ioctl,
};

static struct miscdevice hmdyhub_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "humidity",
	.fops = &hmdyhub_fops,
};
static int hmdyhub_open_report_data(int open)
{
	return 0;
}

static int hmdyhub_enable_nodata(int en)
{
	int res = 0;
	bool power = false;

	if (1 == en)
		power = true;
	if (0 == en)
		power = false;

	res = hmdyhub_set_powermode(power);
	if (res < 0) {
		HMDYHUB_LOG("hmdyhub_set_powermode fail\n");
		return res;
	}
	HMDYHUB_LOG("hmdyhub_set_powermode OK!\n");
	return res;
}

static int hmdyhub_set_delay(u64 ns)
{
	int err = 0;
	unsigned int delayms = 0;
	struct hmdyhub_ipi_data *obj = obj_ipi_data;

	delayms = (unsigned int)ns / 1000 / 1000;
	if (atomic_read(&obj->scp_init_done)) {
		err = sensor_set_delay_to_hub(ID_RELATIVE_HUMIDITY, delayms);
		if (err < 0) {
			HMDYHUB_ERR("als_set_delay fail!\n");
			return err;
		}
	} else {
		HMDYHUB_ERR("sensor hub has not been ready!!\n");
		return -1;
	}
	HMDYHUB_LOG("hmdyhub_set_delay (%d)\n", delayms);

	return 0;
}

static int hmdyhub_get_data(int *value, int *status)
{
	char buff[HMDYHUB_BUFSIZE];
	int err = 0;

	err = hmdyhub_get_humidity(buff, HMDYHUB_BUFSIZE);
	if (err) {
		HMDYHUB_ERR("get compensated humidity value failed," "err = %d\n", err);
		return -1;
	}
	err = kstrtoint(buff, 16, value);
	if (err == 0)
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}
static int scp_ready_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct hmdyhub_ipi_data *obj = obj_ipi_data;

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

static int hmdyhub_probe(struct platform_device *pdev)
{
	struct hmdyhub_ipi_data *obj;
	struct hmdy_control_path ctl = { 0 };
	struct hmdy_data_path data = { 0 };
	int err = 0;

	HMDYHUB_FUN();

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
	err = misc_register(&hmdyhub_misc_device);
	if (err) {
		HMDYHUB_ERR("misc device register failed, err = %d\n", err);
		goto exit_misc_device_register_failed;
	}

	ctl.is_use_common_factory = false;
	err = hmdyhub_create_attr(&(hmdyhub_init_info.platform_diver_addr->driver));
	if (err) {
		HMDYHUB_ERR("create attribute failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = hmdyhub_open_report_data;
	ctl.enable_nodata = hmdyhub_enable_nodata;
	ctl.set_delay = hmdyhub_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = true;

	err = hmdy_register_control_path(&ctl);
	if (err) {
		HMDYHUB_ERR("register hmdy control path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = hmdyhub_get_data;
	data.vender_div = 1000;
	err = hmdy_register_data_path(&data);
	if (err) {
		HMDYHUB_ERR("hmdy_register_data_path failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}
	err = batch_register_support_info(ID_RELATIVE_HUMIDITY, ctl.is_support_batch, data.vender_div, 1);
	if (err) {
		HMDYHUB_ERR("register hmdy batch support err = %d\n", err);
		goto exit_create_attr_failed;
	}

	hmdyhub_init_flag = 0;
	HMDYHUB_LOG("%s: OK\n", __func__);
	return 0;


exit_create_attr_failed:
	hmdyhub_delete_attr(&(hmdyhub_init_info.platform_diver_addr->driver));
exit_misc_device_register_failed:
	misc_deregister(&hmdyhub_misc_device);
	kfree(obj);
exit:
	HMDYHUB_ERR("err = %d\n", err);
	hmdyhub_init_flag = -1;
	return err;
}

static int hmdyhub_remove(struct platform_device *pdev)
{
	int err = 0;

	err = hmdyhub_delete_attr(&(hmdyhub_init_info.platform_diver_addr->driver));
	if (err)
		HMDYHUB_ERR("hmdyhub_delete_attr failed, err = %d\n", err);

	err = misc_deregister(&hmdyhub_misc_device);
	if (err)
		HMDYHUB_ERR("misc_deregister failed, err = %d\n", err);

	obj_ipi_data = NULL;
	kfree(platform_get_drvdata(pdev));

	return 0;
}
static int hmdyhub_suspend(struct platform_device *pdev, pm_message_t msg)
{
	return 0;
}

static int hmdyhub_resume(struct platform_device *pdev)
{
	return 0;
}
static struct platform_device hmdyhub_device = {
	.name = HMDYHUB_DEV_NAME,
	.id = -1,
};

static struct platform_driver hmdyhub_driver = {
	.driver = {
		.name = HMDYHUB_DEV_NAME,
	},
	.probe = hmdyhub_probe,
	.remove = hmdyhub_remove,
	.suspend = hmdyhub_suspend,
	.resume = hmdyhub_resume,
};

static int hmdyhub_local_remove(void)
{
	HMDYHUB_FUN();
	platform_driver_unregister(&hmdyhub_driver);
	return 0;
}

static int hmdyhub_local_init(void)
{
	if (platform_driver_register(&hmdyhub_driver)) {
		HMDYHUB_ERR("add driver error\n");
		return -1;
	}
	if (-1 == hmdyhub_init_flag)
		return -1;
	return 0;
}

static int __init hmdyhub_init(void)
{
	HMDYHUB_FUN();
	if (platform_device_register(&hmdyhub_device)) {
		HMDYHUB_ERR("hmdy platform device error\n");
		return -1;
	}
	hmdy_driver_add(&hmdyhub_init_info);
	return 0;
}

static void __exit hmdyhub_exit(void)
{
	HMDYHUB_FUN();
	platform_driver_unregister(&hmdyhub_driver);
}

module_init(hmdyhub_init);
module_exit(hmdyhub_exit);

MODULE_LICENSE("GPLv2");
MODULE_DESCRIPTION("HMDYHUB Driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
MODULE_VERSION(HMDYHUB_DRIVER_VERSION);
