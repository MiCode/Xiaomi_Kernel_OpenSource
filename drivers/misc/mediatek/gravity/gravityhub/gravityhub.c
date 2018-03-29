/* gravityhub motion sensor driver
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
#include "gravityhub.h"
#include <gravity.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define GRVTY_TAG                  "[gravhub] "
#define GRVTY_FUN(f)               pr_debug(GRVTY_TAG"%s\n", __func__)
#define GRVTY_ERR(fmt, args...)    pr_err(GRVTY_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GRVTY_LOG(fmt, args...)    pr_debug(GRVTY_TAG fmt, ##args)

typedef enum {
	GRAVHUB_TRC_INFO = 0X10,
} GRAVHUB_TRC;

static struct grav_init_info gravityhub_init_info;

struct gravhub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
};

static struct gravhub_ipi_data obj_ipi_data;

static ssize_t show_gravity_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct gravhub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		GRVTY_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		GRVTY_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(gravity, S_IRUGO, show_gravity_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *gravityhub_attr_list[] = {
	&driver_attr_gravity,
	&driver_attr_trace,
};

static int gravityhub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(gravityhub_attr_list) / sizeof(gravityhub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, gravityhub_attr_list[idx]);
		if (0 != err) {
			GRVTY_ERR("driver_create_file (%s) = %d\n", gravityhub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int gravityhub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(gravityhub_attr_list) / sizeof(gravityhub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, gravityhub_attr_list[idx]);

	return err;
}

static int grav_get_data(int *x, int *y, int *z, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_GRAVITY, &data);
	if (err < 0) {
		GRVTY_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp				= data.time_stamp;
	time_stamp_gpt			= data.time_stamp_gpt;
	*x = data.accelerometer_t.x;
	*y = data.accelerometer_t.y;
	*z = data.accelerometer_t.z;
	*status = data.accelerometer_t.status;
	/* GRVTY_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, x: %d, y: %d, z: %d!\n",
			time_stamp, time_stamp_gpt, *x, *y, *z); */
	return 0;
}
static int grav_open_report_data(int open)
{
	return 0;
}
static int grav_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_GRAVITY, en);
}
static int grav_set_delay(u64 delay)
{
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_GRAVITY, delayms);
}
static int gravityhub_local_init(void)
{
	struct grav_control_path ctl = {0};
	struct grav_data_path data = {0};
	int err = 0;

	err = gravityhub_create_attr(&gravityhub_init_info.platform_diver_addr->driver);
	if (err) {
		GRVTY_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = grav_open_report_data;
	ctl.enable_nodata = grav_enable_nodata;
	ctl.set_delay = grav_set_delay;
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
	err = grav_register_control_path(&ctl);
	if (err) {
		GRVTY_ERR("register gravity control path err\n");
		goto exit;
	}

	data.get_data = grav_get_data;
	data.vender_div = 1000;
	err = grav_register_data_path(&data);
	if (err) {
		GRVTY_ERR("register gravity data path err\n");
		goto exit;
	}
	err = batch_register_support_info(ID_GRAVITY, ctl.is_support_batch, data.vender_div, 1);
	if (err) {
		GRVTY_ERR("register magnetic batch support err = %d\n", err);
		goto exit;
	}
	return 0;
exit:
	gravityhub_delete_attr(&(gravityhub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int gravityhub_local_uninit(void)
{
	return 0;
}

static struct grav_init_info gravityhub_init_info = {
	.name = "gravity_hub",
	.init = gravityhub_local_init,
	.uninit = gravityhub_local_uninit,
};

static int __init gravityhub_init(void)
{
	grav_driver_add(&gravityhub_init_info);
	return 0;
}

static void __exit gravityhub_exit(void)
{
	GRVTY_FUN();
}

module_init(gravityhub_init);
module_exit(gravityhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
