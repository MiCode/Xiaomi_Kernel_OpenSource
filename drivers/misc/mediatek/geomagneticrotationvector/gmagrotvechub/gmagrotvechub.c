/* gmagrotvechub motion sensor driver
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
#include "gmagrotvechub.h"
#include <gmrv.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define GMAGROTVEC_TAG                  "[gmagrotvechub] "
#define GMAGROTVEC_FUN(f)               printk(GMAGROTVEC_TAG"%s\n", __func__)
#define GMAGROTVEC_ERR(fmt, args...)    printk(GMAGROTVEC_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GMAGROTVEC_LOG(fmt, args...)    printk(GMAGROTVEC_TAG fmt, ##args)
typedef enum {
	GMAGROTVECHUB_TRC_INFO = 0X10,
} GMAGROTVECHUB_TRC;

static struct gmrv_init_info gmagrotvechub_init_info;

struct gmagrotvechub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
};

static struct gmagrotvechub_ipi_data obj_ipi_data;

static ssize_t show_gmagrotvec_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct gmagrotvechub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		GMAGROTVEC_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		GMAGROTVEC_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(gmagrotvec, S_IRUGO, show_gmagrotvec_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *gmagrotvechub_attr_list[] = {
	&driver_attr_gmagrotvec,
	&driver_attr_trace,
};

static int gmagrotvechub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(gmagrotvechub_attr_list) / sizeof(gmagrotvechub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, gmagrotvechub_attr_list[idx]);
		if (0 != err) {
			GMAGROTVEC_ERR("driver_create_file (%s) = %d\n", gmagrotvechub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int gmagrotvechub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(gmagrotvechub_attr_list) / sizeof(gmagrotvechub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, gmagrotvechub_attr_list[idx]);

	return err;
}

static int gmagrotvec_get_data(int *x, int *y, int *z, int *scalar, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_GEOMAGNETIC_ROTATION_VECTOR, &data);
	if (err < 0) {
		GMAGROTVEC_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*x				= data.magnetic_t.azimuth;
	*y				= data.magnetic_t.pitch;
	*z				= data.magnetic_t.roll;
	*scalar				= data.magnetic_t.scalar;
	*status		= data.magnetic_t.status;
	/* GMAGROTVEC_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, x: %d, y: %d, z: %d!\n",
		time_stamp, time_stamp_gpt, *x, *y, *z); */
	return 0;
}
static int gmagrotvec_open_report_data(int open)
{
	return 0;
}
static int gmagrotvec_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_GEOMAGNETIC_ROTATION_VECTOR, en);
}
static int gmagrotvec_set_delay(u64 delay)
{
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_GEOMAGNETIC_ROTATION_VECTOR, delayms);
}
static int gmagrotvechub_local_init(void)
{
	struct gmrv_control_path ctl = {0};
	struct gmrv_data_path data = {0};
	int err = 0;

	err = gmagrotvechub_create_attr(&gmagrotvechub_init_info.platform_diver_addr->driver);
	if (err) {
		GMAGROTVEC_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = gmagrotvec_open_report_data;
	ctl.enable_nodata = gmagrotvec_enable_nodata;
	ctl.set_delay = gmagrotvec_set_delay;
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
	err = gmrv_register_control_path(&ctl);
	if (err) {
		GMAGROTVEC_ERR("register gmagrotvec control path err\n");
		goto exit;
	}

	data.get_data = gmagrotvec_get_data;
	data.vender_div = 1000000;
	err = gmrv_register_data_path(&data);
	if (err) {
		GMAGROTVEC_ERR("register gmagrotvec data path err\n");
		goto exit;
	}
	err = batch_register_support_info(ID_GEOMAGNETIC_ROTATION_VECTOR, ctl.is_support_batch, data.vender_div, 1);
	if (err) {
		GMAGROTVEC_ERR("register magnetic batch support err = %d\n", err);
		goto exit;
	}
	return 0;
exit:
	gmagrotvechub_delete_attr(&(gmagrotvechub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int gmagrotvechub_local_uninit(void)
{
	return 0;
}

static struct gmrv_init_info gmagrotvechub_init_info = {
	.name = "gmagrotvec_hub",
	.init = gmagrotvechub_local_init,
	.uninit = gmagrotvechub_local_uninit,
};

static int __init gmagrotvechub_init(void)
{
	gmrv_driver_add(&gmagrotvechub_init_info);
	return 0;
}

static void __exit gmagrotvechub_exit(void)
{
	GMAGROTVEC_FUN();
}

module_init(gmagrotvechub_init);
module_exit(gmagrotvechub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
