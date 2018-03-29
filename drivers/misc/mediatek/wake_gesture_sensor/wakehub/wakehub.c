/* wakehub motion sensor driver
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
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>
#include "wakehub.h"
#include <wake_gesture.h>
#include <hwmsen_helper.h>

#include <batch.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define WAKEHUB_TAG                  "[wakehub] "
#define WAKEHUB_FUN(f)               pr_debug(WAKEHUB_TAG"%s\n", __func__)
#define WAKEHUB_ERR(fmt, args...)    pr_err(WAKEHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define WAKEHUB_LOG(fmt, args...)    pr_debug(WAKEHUB_TAG fmt, ##args)

typedef enum {
	WAKEHUB_TRC_INFO = 0X10,
} WAKEHUB_TRC;

static struct wag_init_info wakehub_init_info;

struct wakehub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
	struct work_struct	    wag_work;
};

static struct wakehub_ipi_data obj_ipi_data;
static void wag_work(struct work_struct *work)
{
	WAKEHUB_FUN();
	wag_notify();
}

static ssize_t show_wake_gesture_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct wakehub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		WAKEHUB_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		WAKEHUB_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(wake_gesture, S_IRUGO, show_wake_gesture_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *wakehub_attr_list[] = {
	&driver_attr_wake_gesture,
	&driver_attr_trace,
};

static int wakehub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(wakehub_attr_list) / sizeof(wakehub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, wakehub_attr_list[idx]);
		if (0 != err) {
			WAKEHUB_ERR("driver_create_file (%s) = %d\n", wakehub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int wakehub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(wakehub_attr_list) / sizeof(wakehub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, wakehub_attr_list[idx]);

	return err;
}

static int wake_gesture_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_TILT_DETECTOR, &data);
	if (err < 0) {
		WAKEHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	time_stamp_gpt = data.time_stamp_gpt;
	*probability = data.gesture_data_t.probability;
	WAKEHUB_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, probability: %d!\n", time_stamp, time_stamp_gpt,
		*probability);
	return 0;
}
static int wake_gesture_open_report_data(int open)
{
	int ret = 0;

	WAKEHUB_ERR("%s : enable=%d\n", __func__, open);
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_TILT_DETECTOR, 120);
	ret = sensor_enable_to_hub(ID_TILT_DETECTOR, open);
	return ret;
}
static int SCP_sensorHub_notify_handler(void *data, unsigned int len)
{
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P)data;
	struct wakehub_ipi_data *obj = &obj_ipi_data;

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		WAKEHUB_LOG("sensorId = %d\n", rsp->notify_rsp.sensorType);
		switch (rsp->notify_rsp.event) {
		case SCP_NOTIFY:
			if (ID_TILT_DETECTOR == rsp->notify_rsp.sensorType)
				schedule_work(&(obj->wag_work));
			break;
		default:
			WAKEHUB_ERR("Error sensor hub notify");
		break;
		}
		break;
	default:
		WAKEHUB_ERR("Error sensor hub action");
	break;
	}

	return 0;
}

static int wakehub_local_init(void)
{
	struct wag_control_path ctl = {0};
	struct wag_data_path data = {0};
	struct wakehub_ipi_data *obj = &obj_ipi_data;
	int err = 0;

	err = wakehub_create_attr(&wakehub_init_info.platform_diver_addr->driver);
	if (err) {
		WAKEHUB_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = wake_gesture_open_report_data;
	err = wag_register_control_path(&ctl);
	if (err) {
		WAKEHUB_ERR("register wake_gesture control path err\n");
		goto exit;
	}

	data.get_data = wake_gesture_get_data;
	err = wag_register_data_path(&data);
	if (err) {
		WAKEHUB_ERR("register wake_gesture data path err\n");
		goto exit;
	}
	INIT_WORK(&obj->wag_work, wag_work);
	err = SCP_sensorHub_rsp_registration(ID_WAKE_GESTURE, SCP_sensorHub_notify_handler);
	if (err) {
		WAKEHUB_ERR("SCP_sensorHub_rsp_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
	wakehub_delete_attr(&(wakehub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int wakehub_local_uninit(void)
{
	return 0;
}

static struct wag_init_info wakehub_init_info = {
	.name = "wake_gesture_hub",
	.init = wakehub_local_init,
	.uninit = wakehub_local_uninit,
};

static int __init wakehub_init(void)
{
	wag_driver_add(&wakehub_init_info);
	return 0;
}

static void __exit wakehub_exit(void)
{
	WAKEHUB_FUN();
}

module_init(wakehub_init);
module_exit(wakehub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLANCE_GESTURE_HUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");

