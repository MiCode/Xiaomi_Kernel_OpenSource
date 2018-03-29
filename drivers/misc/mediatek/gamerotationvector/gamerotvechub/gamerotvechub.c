/* gamerotvechub motion sensor driver
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
#include "gamerotvechub.h"
#include <grv.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define GROTVEC_TAG                  "[gamerotvechub] "
#define GROTVEC_FUN(f)               pr_debug(GROTVEC_TAG"%s\n", __func__)
#define GROTVEC_ERR(fmt, args...)    pr_err(GROTVEC_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GROTVEC_LOG(fmt, args...)    pr_debug(GROTVEC_TAG fmt, ##args)

typedef enum {
	GROTVECHUB_TRC_INFO = 0X10,
} GROTVECHUB_TRC;

static struct grv_init_info gamerotvechub_init_info;

struct gamerotvechub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
};

static struct gamerotvechub_ipi_data obj_ipi_data;

static ssize_t show_gamerotvec_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct gamerotvechub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		GROTVEC_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		GROTVEC_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(gamerotvec, S_IRUGO, show_gamerotvec_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *gamerotvechub_attr_list[] = {
	&driver_attr_gamerotvec,
	&driver_attr_trace,
};

static int gamerotvechub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(gamerotvechub_attr_list) / sizeof(gamerotvechub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, gamerotvechub_attr_list[idx]);
		if (0 != err) {
			GROTVEC_ERR("driver_create_file (%s) = %d\n", gamerotvechub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int gamerotvechub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(gamerotvechub_attr_list) / sizeof(gamerotvechub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, gamerotvechub_attr_list[idx]);

	return err;
}

static int gamerotvec_get_data(int *x, int *y, int *z, int *scalar, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_GAME_ROTATION_VECTOR, &data);
	if (err < 0) {
		GROTVEC_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*x				= data.orientation_t.azimuth;
	*y				= data.orientation_t.pitch;
	*z				= data.orientation_t.roll;
	*scalar				= data.orientation_t.scalar;
	*status			= data.orientation_t.status;
	GROTVEC_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, x: %d, y: %d, z: %d!\n",
		time_stamp, time_stamp_gpt, *x, *y, *z);
	return 0;
}
static int gamerotvec_open_report_data(int open)
{
	return 0;
}
static int gamerotvec_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_GAME_ROTATION_VECTOR, en);
}
static int gamerotvec_set_delay(u64 delay)
{
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_GAME_ROTATION_VECTOR, delayms);
}
static int gamerotvechub_local_init(void)
{
	struct grv_control_path ctl = {0};
	struct grv_data_path data = {0};
	int err = 0;

	err = gamerotvechub_create_attr(&gamerotvechub_init_info.platform_diver_addr->driver);
	if (err) {
		GROTVEC_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = gamerotvec_open_report_data;
	ctl.enable_nodata = gamerotvec_enable_nodata;
	ctl.set_delay = gamerotvec_set_delay;
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
	err = grv_register_control_path(&ctl);
	if (err) {
		GROTVEC_ERR("register gamerotvec control path err\n");
		goto exit;
	}

	data.get_data = gamerotvec_get_data;
	data.vender_div = 1000000;
	err = grv_register_data_path(&data);
	if (err) {
		GROTVEC_ERR("register gamerotvec data path err\n");
		goto exit;
	}
	err = batch_register_support_info(ID_GAME_ROTATION_VECTOR, ctl.is_support_batch, data.vender_div, 1);
	if (err) {
		GROTVEC_ERR("register magnetic batch support err = %d\n", err);
		goto exit;
	}
	return 0;
exit:
	gamerotvechub_delete_attr(&(gamerotvechub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int gamerotvechub_local_uninit(void)
{
	return 0;
}

static struct grv_init_info gamerotvechub_init_info = {
	.name = "gamerotvec_hub",
	.init = gamerotvechub_local_init,
	.uninit = gamerotvechub_local_uninit,
};

static int __init gamerotvechub_init(void)
{
	grv_driver_add(&gamerotvechub_init_info);
	return 0;
}

static void __exit gamerotvechub_exit(void)
{
	GROTVEC_FUN();
}

module_init(gamerotvechub_init);
module_exit(gamerotvechub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
