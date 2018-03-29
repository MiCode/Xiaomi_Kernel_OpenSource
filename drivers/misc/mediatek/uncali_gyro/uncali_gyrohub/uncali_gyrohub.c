/* uncali_gyrohub motion sensor driver
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
#include "uncali_gyrohub.h"
#include <uncali_gyro.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define UNGYROHUB_TAG                  "[uncali_gyrohub] "
#define UNGYROHUB_FUN(f)               pr_debug(UNGYROHUB_TAG"%s\n", __func__)
#define UNGYROHUB_ERR(fmt, args...)    pr_err(UNGYROHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define UNGYROHUB_LOG(fmt, args...)    pr_debug(UNGYROHUB_TAG fmt, ##args)

typedef enum {
	UNGYROHUB_TRC_INFO = 0X10,
} UNGYROHUB_TRC;

static struct uncali_gyro_init_info uncali_gyrohub_init_info;

struct uncali_gyrohub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
};

static struct uncali_gyrohub_ipi_data obj_ipi_data;

static ssize_t show_uncali_gyro_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct uncali_gyrohub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		UNGYROHUB_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		UNGYROHUB_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(uncali_gyro, S_IRUGO, show_uncali_gyro_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *uncali_gyrohub_attr_list[] = {
	&driver_attr_uncali_gyro,
	&driver_attr_trace,
};

static int uncali_gyrohub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(uncali_gyrohub_attr_list) / sizeof(uncali_gyrohub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, uncali_gyrohub_attr_list[idx]);
		if (0 != err) {
			UNGYROHUB_ERR("driver_create_file (%s) = %d\n", uncali_gyrohub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int uncali_gyrohub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(uncali_gyrohub_attr_list) / sizeof(uncali_gyrohub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, uncali_gyrohub_attr_list[idx]);

	return err;
}

static int uncali_gyro_get_data(int *dat, int *offset, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_GYROSCOPE_UNCALIBRATED, &data);
	if (err < 0) {
		UNGYROHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp				= data.time_stamp;
	time_stamp_gpt			= data.time_stamp_gpt;
	dat[0] = data.uncalibrated_gyro_t.x;
	dat[1] = data.uncalibrated_gyro_t.y;
	dat[2] = data.uncalibrated_gyro_t.z;
	offset[0] = data.uncalibrated_gyro_t.x_bias;
	offset[1] = data.uncalibrated_gyro_t.y_bias;
	offset[2] = data.uncalibrated_gyro_t.z_bias;
	*status = data.uncalibrated_gyro_t.status;
	/*UNGYROHUB_ERR("x:%d,y:%d,z:%d,x_bias:%d,ybias:%d,z_bias:%d,status:%d\n", dat[0],
			dat[1], dat[2], offset[0], offset[1],
			offset[2], *status);*/
	return 0;
}
static int uncali_gyro_open_report_data(int open)
{
	return 0;
}
static int uncali_gyro_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_GYROSCOPE_UNCALIBRATED, en);
}
static int uncali_gyro_set_delay(u64 delay)
{
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_GYROSCOPE_UNCALIBRATED, delayms);
}
static int uncali_gyrohub_local_init(void)
{
	struct uncali_gyro_control_path ctl = {0};
	struct uncali_gyro_data_path data = {0};
	int err = 0;

	err = uncali_gyrohub_create_attr(&uncali_gyrohub_init_info.platform_diver_addr->driver);
	if (err) {
		UNGYROHUB_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = uncali_gyro_open_report_data;
	ctl.enable_nodata = uncali_gyro_enable_nodata;
	ctl.set_delay = uncali_gyro_set_delay;
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
	err = uncali_gyro_register_control_path(&ctl);
	if (err) {
		UNGYROHUB_ERR("register uncali_gyro control path err\n");
		goto exit;
	}

	data.get_data = uncali_gyro_get_data;
	data.vender_div = 7506;
	err = uncali_gyro_register_data_path(&data);
	if (err) {
		UNGYROHUB_ERR("register uncali_gyro data path err\n");
		goto exit;
	}
	err = batch_register_support_info(ID_GYROSCOPE_UNCALIBRATED, ctl.is_support_batch, data.vender_div, 1);
	if (err) {
		UNGYROHUB_ERR("register gsensor batch support err = %d\n", err);
		goto exit_create_attr_failed;
	}
	return 0;
exit:
	uncali_gyrohub_delete_attr(&(uncali_gyrohub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int uncali_gyrohub_local_uninit(void)
{
	return 0;
}

static struct uncali_gyro_init_info uncali_gyrohub_init_info = {
	.name = "uncali_gyro_hub",
	.init = uncali_gyrohub_local_init,
	.uninit = uncali_gyrohub_local_uninit,
};

static int __init uncali_gyrohub_init(void)
{
	uncali_gyro_driver_add(&uncali_gyrohub_init_info);
	return 0;
}

static void __exit uncali_gyrohub_exit(void)
{
	UNGYROHUB_FUN();
}

module_init(uncali_gyrohub_init);
module_exit(uncali_gyrohub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
