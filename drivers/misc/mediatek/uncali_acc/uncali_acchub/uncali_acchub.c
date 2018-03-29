/* uncali_acchub motion sensor driver
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
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "uncali_acchub.h"
#include <uncali_acc.h>
#include <linux/hwmsen_helper.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include <linux/batch.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define UNACCHUB_TAG                  "[uncali_acchub] "
#define UNACCHUB_FUN(f)               printk(UNACCHUB_TAG"%s\n", __func__)
#define UNACCHUB_ERR(fmt, args...)    printk(UNACCHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define UNACCHUB_LOG(fmt, args...)    printk(UNACCHUB_TAG fmt, ##args)

typedef enum {
	UNACCHUB_TRC_INFO = 0X10,
} UNACCHUB_TRC;

static struct uncali_acc_init_info uncali_acchub_init_info;

struct uncali_acchub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
};

static struct uncali_acchub_ipi_data obj_ipi_data;

static ssize_t show_uncali_acc_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct uncali_acchub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		UNACCHUB_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		UNACCHUB_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(uncali_acc, S_IRUGO, show_uncali_acc_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *uncali_acchub_attr_list[] = {
	&driver_attr_uncali_acc,
	&driver_attr_trace,
};

static int uncali_acchub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(uncali_acchub_attr_list) / sizeof(uncali_acchub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, uncali_acchub_attr_list[idx]);
		if (0 != err) {
			UNACCHUB_ERR("driver_create_file (%s) = %d\n", uncali_acchub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int uncali_acchub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(uncali_acchub_attr_list) / sizeof(uncali_acchub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;
	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, uncali_acchub_attr_list[idx]);

	return err;
}

static int uncali_acc_get_data(int *dat, int *offset, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_ACCELEROMETER_UNCALIBRATED, &data);
	if (err < 0) {
		UNACCHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp				= data.time_stamp;
	time_stamp_gpt			= data.time_stamp_gpt;
	dat[0] = data.uncalibrated_acc_t.x_uncali;
	dat[1] = data.uncalibrated_acc_t.y_uncali;
	dat[2] = data.uncalibrated_acc_t.z_uncali;
	offset[0] = data.uncalibrated_acc_t.x_bias;
	offset[1] = data.uncalibrated_acc_t.y_bias;
	offset[2] = data.uncalibrated_acc_t.z_bias;

	return 0;
}
static int uncali_acc_open_report_data(int open)
{
	return 0;
}
static int uncali_acc_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_ACCELEROMETER_UNCALIBRATED, en);
}
static int uncali_acc_set_delay(u64 delay)
{
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_ACCELEROMETER_UNCALIBRATED, delayms);
}
static int uncali_acchub_local_init(void)
{
	struct uncali_acc_control_path ctl = {0};
	struct uncali_acc_data_path data = {0};
	int err = 0;

	err = uncali_acchub_create_attr(&uncali_acchub_init_info.platform_diver_addr->driver);
	if (err) {
		UNACCHUB_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = uncali_acc_open_report_data;
	ctl.enable_nodata = uncali_acc_enable_nodata;
	ctl.set_delay = uncali_acc_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = true;
	err = uncali_acc_register_control_path(&ctl);
	if (err) {
		UNACCHUB_ERR("register uncali_acc control path err\n");
		goto exit;
	}

	data.get_data = uncali_acc_get_data;
	err = uncali_acc_register_data_path(&data);
	if (err) {
		UNACCHUB_ERR("register uncali_acc data path err\n");
		goto exit;
	}
	return 0;
exit:
	uncali_acchub_delete_attr(&(uncali_acchub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int uncali_acchub_local_uninit(void)
{
	return 0;
}

static struct uncali_acc_init_info uncali_acchub_init_info = {
	.name = "uncali_acc_hub",
	.init = uncali_acchub_local_init,
	.uninit = uncali_acchub_local_uninit,
};

static int __init uncali_acchub_init(void)
{
	uncali_acc_driver_add(&uncali_acchub_init_info);
	return 0;
}

static void __exit uncali_acchub_exit(void)
{
	UNACCHUB_FUN();
}

module_init(uncali_acchub_init);
module_exit(uncali_acchub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
