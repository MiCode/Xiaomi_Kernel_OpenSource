/* pkuphub motion sensor driver
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
#include "pickuphub.h"
#include <pick_up.h>
#include <hwmsen_helper.h>


#include <batch.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define PKUPHUB_TAG                  "[pkuphub] "
#define PKUPHUB_FUN(f)               printk(PKUPHUB_TAG"%s\n", __func__)
#define PKUPHUB_ERR(fmt, args...)    printk(PKUPHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define PKUPHUB_LOG(fmt, args...)    printk(PKUPHUB_TAG fmt, ##args)

typedef enum {
	PKUPHUB_TRC_INFO = 0X10,
} PKUPHUB_TRC;

static struct pkup_init_info pkuphub_init_info;

struct pkuphub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
	struct work_struct pkup_work;
};

static struct pkuphub_ipi_data obj_ipi_data;
static void pkup_work(struct work_struct *work)
{
	PKUPHUB_FUN();
	pkup_notify();
}

static ssize_t show_pickup_gesture_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct pkuphub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		PKUPHUB_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		PKUPHUB_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(pickup_gesture, S_IRUGO, show_pickup_gesture_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *pkuphub_attr_list[] = {
	&driver_attr_pickup_gesture,
	&driver_attr_trace,
};

static int pkuphub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(pkuphub_attr_list) / sizeof(pkuphub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, pkuphub_attr_list[idx]);
		if (0 != err) {
			PKUPHUB_ERR("driver_create_file (%s) = %d\n",
				    pkuphub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int pkuphub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(pkuphub_attr_list) / sizeof(pkuphub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, pkuphub_attr_list[idx]);

	return err;
}

static int pickup_gesture_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_PICK_UP_GESTURE, &data);
	if (err < 0) {
		PKUPHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	time_stamp_gpt = data.time_stamp_gpt;
	*probability = data.gesture_data_t.probability;
	PKUPHUB_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, probability: %d!\n",
		    time_stamp, time_stamp_gpt, *probability);
	return 0;
}

static int pickup_gesture_open_report_data(int open)
{
	int ret = 0;

	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_PICK_UP_GESTURE, 120);
	ret = sensor_enable_to_hub(ID_PICK_UP_GESTURE, open);
	return ret;
}

static int SCP_sensorHub_notify_handler(void *data, unsigned int len)
{
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;
	struct pkuphub_ipi_data *obj = &obj_ipi_data;

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		PKUPHUB_LOG("sensorId = %d\n", rsp->notify_rsp.sensorType);
		switch (rsp->notify_rsp.event) {
		case SCP_NOTIFY:
			if (ID_PICK_UP_GESTURE == rsp->notify_rsp.sensorType)
				schedule_work(&(obj->pkup_work));
			break;
		default:
			PKUPHUB_ERR("Error sensor hub notify");
			break;
		}
		break;
	default:
		PKUPHUB_ERR("Error sensor hub action");
		break;
	}

	return 0;
}

static int pkuphub_local_init(void)
{
	struct pkup_control_path ctl = { 0 };
	struct pkup_data_path data = { 0 };
	struct pkuphub_ipi_data *obj = &obj_ipi_data;
	int err = 0;

	err = pkuphub_create_attr(&pkuphub_init_info.platform_diver_addr->driver);
	if (err) {
		PKUPHUB_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = pickup_gesture_open_report_data;
	err = pkup_register_control_path(&ctl);
	if (err) {
		PKUPHUB_ERR("register pickup_gesture control path err\n");
		goto exit;
	}

	data.get_data = pickup_gesture_get_data;
	err = pkup_register_data_path(&data);
	if (err) {
		PKUPHUB_ERR("register pickup_gesture data path err\n");
		goto exit;
	}
	INIT_WORK(&obj->pkup_work, pkup_work);
	err = SCP_sensorHub_rsp_registration(ID_PICK_UP_GESTURE, SCP_sensorHub_notify_handler);
	if (err) {
		PKUPHUB_ERR("SCP_sensorHub_rsp_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
	pkuphub_delete_attr(&(pkuphub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}

static int pkuphub_local_uninit(void)
{
	return 0;
}

static struct pkup_init_info pkuphub_init_info = {
	.name = "pickup_gesture_hub",
	.init = pkuphub_local_init,
	.uninit = pkuphub_local_uninit,
};

static int __init pkuphub_init(void)
{
	pkup_driver_add(&pkuphub_init_info);
	return 0;
}

static void __exit pkuphub_exit(void)
{
	PKUPHUB_FUN();
}

module_init(pkuphub_init);
module_exit(pkuphub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLANCE_GESTURE_HUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
