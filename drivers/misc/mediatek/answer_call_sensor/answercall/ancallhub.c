/* ancallhub motion sensor driver
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
#include "ancallhub.h"
#include <answer_call.h>
#include <hwmsen_helper.h>

#include <batch.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define ANCALLHUB_TAG                  "[ancallhub] "
#define ANCALLHUB_FUN(f)               printk(ANCALLHUB_TAG"%s\n", __func__)
#define ANCALLHUB_ERR(fmt, args...)    printk(ANCALLHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define ANCALLHUB_LOG(fmt, args...)    printk(ANCALLHUB_TAG fmt, ##args)

typedef enum {
	ANCALLHUBH_TRC_INFO = 0X10,
} ANCALLHUB_TRC;

static struct ancall_init_info ancallhub_init_info;

struct ancallhub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
	struct work_struct	    ancall_work;
};

static struct ancallhub_ipi_data obj_ipi_data;
static void ancall_work(struct work_struct *work)
{
	ANCALLHUB_FUN();
	ancall_notify();
}

static ssize_t show_answer_call_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct ancallhub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		ANCALLHUB_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		ANCALLHUB_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(answer_call, S_IRUGO, show_answer_call_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *ancallhub_attr_list[] = {
	&driver_attr_answer_call,
	&driver_attr_trace,
};

static int ancallhub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(ancallhub_attr_list) / sizeof(ancallhub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, ancallhub_attr_list[idx]);
		if (0 != err) {
			ANCALLHUB_ERR("driver_create_file (%s) = %d\n", ancallhub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int ancallhub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(ancallhub_attr_list) / sizeof(ancallhub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, ancallhub_attr_list[idx]);

	return err;
}

static int answer_call_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_ANSWER_CALL, &data);
	if (err < 0) {
		ANCALLHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*probability	= data.gesture_data_t.probability;
	ANCALLHUB_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, probability: %d!\n", time_stamp, time_stamp_gpt,
		*probability);
	return 0;
}
static int answer_call_open_report_data(int open)
{
	int ret = 0;

	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_ANSWER_CALL, 66);
	pr_warn("%s : type=%d, open=%d\n", __func__, ID_ANSWER_CALL, open);
	ret = sensor_enable_to_hub(ID_ANSWER_CALL, open);
	return ret;
}
static int SCP_sensorHub_notify_handler(void *data, unsigned int len)
{
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P)data;
	struct ancallhub_ipi_data *obj = &obj_ipi_data;

	if (SENSOR_HUB_NOTIFY == rsp->rsp.action) {
		ANCALLHUB_LOG("sensorId = %d\n", rsp->notify_rsp.sensorType);
		switch (rsp->notify_rsp.event) {
		case SCP_NOTIFY:
		    if (ID_ANSWER_CALL == rsp->notify_rsp.sensorType)
			schedule_work(&(obj->ancall_work));
			break;
		default:
		    ANCALLHUB_ERR("Error sensor hub notify");
			break;
	    }
	} else
		ANCALLHUB_ERR("Error sensor hub action");

	return 0;
}

static int ancallhub_local_init(void)
{
	struct ancall_control_path ctl = {0};
	struct ancall_data_path data = {0};
	struct ancallhub_ipi_data *obj = &obj_ipi_data;
	int err = 0;

	err = ancallhub_create_attr(&ancallhub_init_info.platform_diver_addr->driver);
	if (err) {
		ANCALLHUB_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = answer_call_open_report_data;
	err = ancall_register_control_path(&ctl);
	if (err) {
		ANCALLHUB_ERR("register answer_call control path err\n");
		goto exit;
	}

	data.get_data = answer_call_get_data;
	err = ancall_register_data_path(&data);
	if (err) {
		ANCALLHUB_ERR("register answer_call data path err\n");
		goto exit;
	}
	INIT_WORK(&obj->ancall_work, ancall_work);
	err = SCP_sensorHub_rsp_registration(ID_ANSWER_CALL, SCP_sensorHub_notify_handler);
	if (err) {
		ANCALLHUB_ERR("SCP_sensorHub_rsp_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
	ancallhub_delete_attr(&(ancallhub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int ancallhub_local_uninit(void)
{
	return 0;
}

static struct ancall_init_info ancallhub_init_info = {
	.name = "answer_call_hub",
	.init = ancallhub_local_init,
	.uninit = ancallhub_local_uninit,
};

static int __init ancallhub_init(void)
{
	ancall_driver_add(&ancallhub_init_info);
	return 0;
}

static void __exit ancallhub_exit(void)
{
	ANCALLHUB_FUN();
}

module_init(ancallhub_init);
module_exit(ancallhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ANSWER_CALL_HUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
