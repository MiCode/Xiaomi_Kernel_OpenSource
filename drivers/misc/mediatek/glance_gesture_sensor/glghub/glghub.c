/* glghub motion sensor driver
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
#include "glghub.h"
#include <glance_gesture.h>
#include <hwmsen_helper.h>

#include <batch.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define GLGHUB_TAG                  "[glghub] "
#define GLGHUB_FUN(f)               printk(GLGHUB_TAG"%s\n", __func__)
#define GLGHUB_ERR(fmt, args...)    printk(GLGHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GLGHUB_LOG(fmt, args...)    printk(GLGHUB_TAG fmt, ##args)

typedef enum {
	GLGHUBH_TRC_INFO = 0X10,
} GLGHUB_TRC;

static struct glg_init_info glghub_init_info;

struct glghub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
	struct work_struct	    glg_work;
};

static struct glghub_ipi_data obj_ipi_data;
static void glg_work(struct work_struct *work)
{
	GLGHUB_FUN();
	glg_notify();
}

static ssize_t show_glance_gesture_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct glghub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		GLGHUB_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		GLGHUB_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(glance_gesture, S_IRUGO, show_glance_gesture_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *glghub_attr_list[] = {
	&driver_attr_glance_gesture,
	&driver_attr_trace,
};

static int glghub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(glghub_attr_list) / sizeof(glghub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, glghub_attr_list[idx]);
		if (0 != err) {
			GLGHUB_ERR("driver_create_file (%s) = %d\n", glghub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int glghub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(glghub_attr_list) / sizeof(glghub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, glghub_attr_list[idx]);

	return err;
}

static int glance_gesture_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_GLANCE_GESTURE, &data);
	if (err < 0) {
		GLGHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*probability	= data.gesture_data_t.probability;
	GLGHUB_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, probability: %d!\n", time_stamp, time_stamp_gpt,
		*probability);
	return 0;
}
static int glance_gesture_open_report_data(int open)
{
	int ret = 0;

	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_GLANCE_GESTURE, 120);
	ret = sensor_enable_to_hub(ID_GLANCE_GESTURE, open);
	return ret;
}
static int SCP_sensorHub_notify_handler(void *data, unsigned int len)
{
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P)data;
	struct glghub_ipi_data *obj = &obj_ipi_data;

	if (SENSOR_HUB_NOTIFY == rsp->rsp.action) {
		GLGHUB_LOG("sensorId = %d\n", rsp->notify_rsp.sensorType);
		switch (rsp->notify_rsp.event) {
		case SCP_NOTIFY:
		    if (ID_GLANCE_GESTURE == rsp->notify_rsp.sensorType)
			schedule_work(&(obj->glg_work));
			break;
		default:
		    GLGHUB_ERR("Error sensor hub notify");
			break;
	    }
	} else
		GLGHUB_ERR("Error sensor hub action");

	return 0;
}

static int glghub_local_init(void)
{
	struct glg_control_path ctl = {0};
	struct glg_data_path data = {0};
	struct glghub_ipi_data *obj = &obj_ipi_data;
	int err = 0;

	err = glghub_create_attr(&glghub_init_info.platform_diver_addr->driver);
	if (err) {
		GLGHUB_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = glance_gesture_open_report_data;
	err = glg_register_control_path(&ctl);
	if (err) {
		GLGHUB_ERR("register glance_gesture control path err\n");
		goto exit;
	}

	data.get_data = glance_gesture_get_data;
	err = glg_register_data_path(&data);
	if (err) {
		GLGHUB_ERR("register glance_gesture data path err\n");
		goto exit;
	}
	INIT_WORK(&obj->glg_work, glg_work);
	err = SCP_sensorHub_rsp_registration(ID_GLANCE_GESTURE, SCP_sensorHub_notify_handler);
	if (err) {
		GLGHUB_ERR("SCP_sensorHub_rsp_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
	glghub_delete_attr(&(glghub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int glghub_local_uninit(void)
{
	return 0;
}

static struct glg_init_info glghub_init_info = {
	.name = "glance_gesture_hub",
	.init = glghub_local_init,
	.uninit = glghub_local_uninit,
};

static int __init glghub_init(void)
{
	glg_driver_add(&glghub_init_info);
	return 0;
}

static void __exit glghub_exit(void)
{
	GLGHUB_FUN();
}

module_init(glghub_init);
module_exit(glghub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLANCE_GESTURE_HUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
