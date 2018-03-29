/* linearaccityhub motion sensor driver
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
#include "linearacchub.h"
#include <linearacceleration.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define LNACC_TAG                  "[lacchub] "
#define LNACC_FUN(f)               printk(LNACC_TAG"%s\n", __func__)
#define LNACC_ERR(fmt, args...)    printk(LNACC_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define LNACC_LOG(fmt, args...)    printk(LNACC_TAG fmt, ##args)

typedef enum {
	LNACCHUB_TRC_INFO = 0X10,
} LNACCHUB_TRC;

static struct la_init_info linearacchub_init_info;

struct linearacchub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
};

static struct linearacchub_ipi_data obj_ipi_data;

static ssize_t show_linearacc_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct linearacchub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		LNACC_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		LNACC_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(linearacc, S_IRUGO, show_linearacc_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *linearacchub_attr_list[] = {
	&driver_attr_linearacc,
	&driver_attr_trace,
};

static int linearacchub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(linearacchub_attr_list) / sizeof(linearacchub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, linearacchub_attr_list[idx]);
		if (0 != err) {
			LNACC_ERR("driver_create_file (%s) = %d\n", linearacchub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int linearacchub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(linearacchub_attr_list) / sizeof(linearacchub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, linearacchub_attr_list[idx]);

	return err;
}

static int linearacc_get_data(int *x, int *y, int *z, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_LINEAR_ACCELERATION, &data);
	if (err < 0) {
		LNACC_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp				= data.time_stamp;
	time_stamp_gpt			= data.time_stamp_gpt;
	*x = data.accelerometer_t.x;
	*y = data.accelerometer_t.y;
	*z = data.accelerometer_t.z;
	*status = data.accelerometer_t.status;
	/* LNACC_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, x: %d, y: %d, z: %d!\n",
			time_stamp, time_stamp_gpt, *x, *y, *z); */
	return 0;
}
static int linearacc_open_report_data(int open)
{
	return 0;
}
static int linearacc_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_LINEAR_ACCELERATION, en);
}
static int linearacc_set_delay(u64 delay)
{
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_LINEAR_ACCELERATION, delayms);
}
static int linearacchub_local_init(void)
{
	struct la_control_path ctl = {0};
	struct la_data_path data = {0};
	int err = 0;

	err = linearacchub_create_attr(&linearacchub_init_info.platform_diver_addr->driver);
	if (err) {
		LNACC_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = linearacc_open_report_data;
	ctl.enable_nodata = linearacc_enable_nodata;
	ctl.set_delay = linearacc_set_delay;
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
	err = la_register_control_path(&ctl);
	if (err) {
		LNACC_ERR("register linearacc control path err\n");
		goto exit;
	}

	data.get_data = linearacc_get_data;
	data.vender_div = 1000;
	err = la_register_data_path(&data);
	if (err) {
		LNACC_ERR("register linearacc data path err\n");
		goto exit;
	}
	err = batch_register_support_info(ID_LINEAR_ACCELERATION, ctl.is_support_batch, data.vender_div, 1);
	if (err) {
		LNACC_ERR("register magnetic batch support err = %d\n", err);
		goto exit;
	}
	return 0;
exit:
	linearacchub_delete_attr(&(linearacchub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int linearacchub_local_uninit(void)
{
	return 0;
}

static struct la_init_info linearacchub_init_info = {
	.name = "linearacc_hub",
	.init = linearacchub_local_init,
	.uninit = linearacchub_local_uninit,
};

static int __init linearacchub_init(void)
{
	la_driver_add(&linearacchub_init_info);
	return 0;
}

static void __exit linearacchub_exit(void)
{
	LNACC_FUN();
}

module_init(linearacchub_init);
module_exit(linearacchub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
