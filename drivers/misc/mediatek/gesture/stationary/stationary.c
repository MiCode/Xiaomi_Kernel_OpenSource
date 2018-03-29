/* stationary gesture sensor driver
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
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>

#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>
#include "gesture.h"
#include "stationary.h"
#include <hwmsen_helper.h>

#include <batch.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define STATHUB_TAG                  "[stationary] "
#define STATHUB_FUN(f)               pr_warn(STATHUB_TAG"%s\n", __func__)
#define STATHUB_ERR(fmt, args...)    pr_warn(STATHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define STATHUB_LOG(fmt, args...)    pr_warn(STATHUB_TAG fmt, ##args)


static struct ges_init_info stat_init_info;

struct stat_ipi_data {
	atomic_t trace;
	atomic_t suspend;
	struct work_struct	 stat_work;
};

static struct stat_ipi_data obj_ipi_data;
static void stat_work(struct work_struct *work)
{
	STATHUB_FUN();
	ges_notify(ID_STATIONARY);
}

static int stat_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_STATIONARY, &data);
	if (err < 0) {
		STATHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*probability	= data.gesture_data_t.probability;
	STATHUB_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, probability: %d!\n", time_stamp, time_stamp_gpt,
		*probability);
	return 0;
}
static int stat_open_report_data(int open)
{
	int ret = 0;

	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_STATIONARY, 120);
	STATHUB_LOG("%s : type=%d, open=%d\n", __func__, ID_STATIONARY, open);
	ret = sensor_enable_to_hub(ID_STATIONARY, open);
	return ret;
}
static int SCP_sensorHub_notify_handler(void *data, unsigned int len)
{
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P)data;
	struct stat_ipi_data *obj = &obj_ipi_data;

	if (SENSOR_HUB_NOTIFY == rsp->rsp.action) {
		STATHUB_LOG("sensorId = %d\n", rsp->notify_rsp.sensorType);
		switch (rsp->notify_rsp.event) {
		case SCP_NOTIFY:
		    if (ID_STATIONARY == rsp->notify_rsp.sensorType)
			schedule_work(&(obj->stat_work));
			break;
		default:
		    STATHUB_ERR("Error sensor hub notify");
			break;
	    }
	} else
		STATHUB_ERR("Error sensor hub action");

	return 0;
}

static int stat_local_init(void)
{
	struct ges_control_path ctl = {0};
	struct ges_data_path data = {0};
	struct stat_ipi_data *obj = &obj_ipi_data;
	int err = 0;

	ctl.open_report_data = stat_open_report_data;
	err = ges_register_control_path(&ctl, ID_STATIONARY);
	if (err) {
		STATHUB_ERR("register stationary control path err\n");
		goto exit;
	}

	data.get_data = stat_get_data;
	err = ges_register_data_path(&data, ID_STATIONARY);
	if (err) {
		STATHUB_ERR("register stationary data path err\n");
		goto exit;
	}
	INIT_WORK(&obj->stat_work, stat_work);
	err = SCP_sensorHub_rsp_registration(ID_STATIONARY, SCP_sensorHub_notify_handler);
	if (err) {
		STATHUB_ERR("SCP_sensorHub_rsp_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int stat_local_uninit(void)
{
	return 0;
}

static struct ges_init_info stat_init_info = {
	.name = "stat_hub",
	.init = stat_local_init,
	.uninit = stat_local_uninit,
};

static int __init stat_init(void)
{
	ges_driver_add(&stat_init_info, ID_STATIONARY);
	return 0;
}

static void __exit stat_exit(void)
{
	STATHUB_FUN();
}

module_init(stat_init);
module_exit(stat_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Stationary Gesture driver");
MODULE_AUTHOR("qiangming.xia@mediatek.com");
