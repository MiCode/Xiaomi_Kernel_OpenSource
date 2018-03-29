/* uncali_maghub motion sensor driver
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
#include "uncali_maghub.h"
#include <uncali_mag.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define UNMAGHUB_TAG                  "[uncali_maghub] "
#define UNMAGHUB_FUN(f)               pr_err(UNMAGHUB_TAG"%s\n", __func__)
#define UNMAGHUB_ERR(fmt, args...)    pr_err(UNMAGHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define UNMAGHUB_LOG(fmt, args...)    pr_err(UNMAGHUB_TAG fmt, ##args)

typedef enum {
	UNMAGHUB_TRC_INFO = 0X10,
} UNMAGHUB_TRC;

static struct uncali_mag_init_info uncali_maghub_init_info;

struct uncali_maghub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
};

static struct uncali_maghub_ipi_data obj_ipi_data;

static ssize_t show_uncali_mag_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct uncali_maghub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		UNMAGHUB_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		UNMAGHUB_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(uncali_mag, S_IRUGO, show_uncali_mag_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *uncali_maghub_attr_list[] = {
	&driver_attr_uncali_mag,
	&driver_attr_trace,
};

static int uncali_maghub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(uncali_maghub_attr_list) / sizeof(uncali_maghub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, uncali_maghub_attr_list[idx]);
		if (0 != err) {
			UNMAGHUB_ERR("driver_create_file (%s) = %d\n", uncali_maghub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int uncali_maghub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(uncali_maghub_attr_list) / sizeof(uncali_maghub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, uncali_maghub_attr_list[idx]);

	return err;
}

static int uncali_mag_get_data(int *dat, int *offset, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_MAGNETIC_UNCALIBRATED, &data);
	if (err < 0) {
		UNMAGHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	time_stamp_gpt = data.time_stamp_gpt;
	dat[0] = data.uncalibrated_mag_t.x;
	dat[1] = data.uncalibrated_mag_t.y;
	dat[2] = data.uncalibrated_mag_t.z;
	offset[0] = data.uncalibrated_mag_t.x_bias;
	offset[1] = data.uncalibrated_mag_t.y_bias;
	offset[2] = data.uncalibrated_mag_t.z_bias;
	*status = data.uncalibrated_mag_t.status;
	return 0;
}
static int uncali_mag_open_report_data(int open)
{
	return 0;
}
static int uncali_mag_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_MAGNETIC_UNCALIBRATED, en);
}
static int uncali_mag_set_delay(u64 delay)
{
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_MAGNETIC_UNCALIBRATED, delayms);
}
static int uncali_maghub_local_init(void)
{
	struct uncali_mag_control_path ctl = {0};
	struct uncali_mag_data_path data = {0};
	int err = 0;

	err = uncali_maghub_create_attr(&uncali_maghub_init_info.platform_diver_addr->driver);
	if (err) {
		UNMAGHUB_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = uncali_mag_open_report_data;
	ctl.enable_nodata = uncali_mag_enable_nodata;
	ctl.set_delay = uncali_mag_set_delay;
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
	err = uncali_mag_register_control_path(&ctl);
	if (err) {
		UNMAGHUB_ERR("register uncali_mag control path err\n");
		goto exit;
	}

	data.get_data = uncali_mag_get_data;
	data.vender_div = 100;
	err = uncali_mag_register_data_path(&data);
	if (err) {
		UNMAGHUB_ERR("register uncali_mag data path err\n");
		goto exit;
	}
	err = batch_register_support_info(ID_MAGNETIC_UNCALIBRATED, ctl.is_support_batch, data.vender_div, 1);
	if (err) {
		UNMAGHUB_ERR("register magnetic batch support err = %d\n", err);
		goto exit_create_attr_failed;
	}
	return 0;
exit:
	uncali_maghub_delete_attr(&(uncali_maghub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int uncali_maghub_local_uninit(void)
{
	return 0;
}

static struct uncali_mag_init_info uncali_maghub_init_info = {
	.name = "uncali_mag_hub",
	.init = uncali_maghub_local_init,
	.uninit = uncali_maghub_local_uninit,
};

static int __init uncali_maghub_init(void)
{
	uncali_mag_driver_add(&uncali_maghub_init_info);
	return 0;
}

static void __exit uncali_maghub_exit(void)
{
	UNMAGHUB_FUN();
}

module_init(uncali_maghub_init);
module_exit(uncali_maghub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");

